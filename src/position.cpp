#include "position.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include "movegen.h"

namespace chess {

namespace zobrist {

Key piece_square[PIECE_NB][SQUARE_NB];
Key castling[16];
Key en_passant_file[8];
Key side;

void init() {
    std::uint64_t s = 0x2545F4914F6CDD1DULL;
    auto next = [&s] {
        // splitmix64
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };
    for (auto& piece : piece_square)
        for (auto& k : piece) k = next();
    for (auto& k : castling) k = next();
    for (auto& k : en_passant_file) k = next();
    side = next();
}

}  // namespace zobrist

namespace {

// Castling rights that survive a move touching each square.
constexpr int castling_mask(int sq) {
    switch (sq) {
        case A1: return ALL_CASTLING & ~WHITE_OOO;
        case E1: return ALL_CASTLING & ~(WHITE_OO | WHITE_OOO);
        case H1: return ALL_CASTLING & ~WHITE_OO;
        case A8: return ALL_CASTLING & ~BLACK_OOO;
        case E8: return ALL_CASTLING & ~(BLACK_OO | BLACK_OOO);
        case H8: return ALL_CASTLING & ~BLACK_OO;
        default: return ALL_CASTLING;
    }
}

constexpr std::string_view PIECE_CHARS = "PNBRQKpnbrqk";

// The en passant file only enters the hash when a capture is actually possible, so that
// transpositions reached with and without a "dead" double push hash identically.
bool ep_capturable(const Position& pos, Square ep) {
    if (ep == NO_SQUARE) return false;
    const Color us = pos.side_to_move();
    return attacks::pawn(~us, ep) & pos.pieces(us, PAWN);
}

}  // namespace

std::string square_to_string(int sq) {
    if (sq < 0 || sq >= SQUARE_NB) return "-";
    return {char('a' + file_of(sq)), char('1' + rank_of(sq))};
}

std::string Move::to_uci() const {
    if (!*this) return "0000";
    std::string s = square_to_string(from()) + square_to_string(to());
    if (is_promotion()) s += "nbrq"[promotion_type() - KNIGHT];
    return s;
}

Position::Position() { set_fen(START_FEN); }

Position::Position(std::string_view fen) { set_fen(fen); }

void Position::clear() {
    for (auto& p : board_) p = NO_PIECE;
    for (auto& b : by_type_) b = 0;
    for (auto& b : by_color_) b = 0;
    side_ = WHITE;
    fullmove_ = 1;
    history_.clear();
}

void Position::put_piece(Piece p, int sq) {
    board_[sq] = p;
    by_type_[type_of(p)] |= square_bb(sq);
    by_color_[color_of(p)] |= square_bb(sq);
}

void Position::remove_piece(int sq) {
    const Piece p = board_[sq];
    by_type_[type_of(p)] &= ~square_bb(sq);
    by_color_[color_of(p)] &= ~square_bb(sq);
    board_[sq] = NO_PIECE;
}

void Position::move_piece(int from, int to) {
    const Piece p = board_[from];
    const Bitboard ft = square_bb(from) | square_bb(to);
    by_type_[type_of(p)] ^= ft;
    by_color_[color_of(p)] ^= ft;
    board_[from] = NO_PIECE;
    board_[to] = p;
}

void Position::set_fen(std::string_view fen) {
    clear();
    std::istringstream in{std::string(fen)};
    std::string board, side, castling = "-", ep = "-";
    int halfmove = 0, fullmove = 1;
    if (!(in >> board >> side)) throw std::invalid_argument("FEN: missing fields");
    in >> castling >> ep;
    in >> halfmove >> fullmove;

    int rank = 7, file = 0;
    for (char c : board) {
        if (c == '/') {
            if (file != 8) throw std::invalid_argument("FEN: bad rank length");
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            const auto idx = PIECE_CHARS.find(c);
            if (idx == std::string_view::npos || file > 7 || rank < 0)
                throw std::invalid_argument("FEN: bad piece placement");
            put_piece(Piece(idx), make_square(file, rank));
            ++file;
        }
        if (file > 8) throw std::invalid_argument("FEN: rank overflow");
    }
    if (rank != 0 || file != 8) throw std::invalid_argument("FEN: incomplete board");
    if (popcount(pieces(WHITE, KING)) != 1 || popcount(pieces(BLACK, KING)) != 1)
        throw std::invalid_argument("FEN: each side needs exactly one king");

    if (side == "w") side_ = WHITE;
    else if (side == "b") side_ = BLACK;
    else throw std::invalid_argument("FEN: bad side to move");

    StateInfo s{};
    s.castling = NO_CASTLING;
    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': s.castling |= WHITE_OO; break;
                case 'Q': s.castling |= WHITE_OOO; break;
                case 'k': s.castling |= BLACK_OO; break;
                case 'q': s.castling |= BLACK_OOO; break;
                default: throw std::invalid_argument("FEN: bad castling field");
            }
        }
    }
    // Drop rights that are inconsistent with the piece placement.
    if (board_[E1] != W_KING) s.castling &= ~(WHITE_OO | WHITE_OOO);
    if (board_[H1] != W_ROOK) s.castling &= ~WHITE_OO;
    if (board_[A1] != W_ROOK) s.castling &= ~WHITE_OOO;
    if (board_[E8] != B_KING) s.castling &= ~(BLACK_OO | BLACK_OOO);
    if (board_[H8] != B_ROOK) s.castling &= ~BLACK_OO;
    if (board_[A8] != B_ROOK) s.castling &= ~BLACK_OOO;

    s.ep_square = NO_SQUARE;
    if (ep != "-") {
        if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || (ep[1] != '3' && ep[1] != '6'))
            throw std::invalid_argument("FEN: bad en passant square");
        s.ep_square = make_square(ep[0] - 'a', ep[1] - '1');
    }
    s.halfmove_clock = halfmove;
    s.plies_from_null = 0;
    s.captured = NO_PIECE;
    fullmove_ = std::max(1, fullmove);
    history_.push_back(s);

    st().key = compute_key();
    st().pawn_key = compute_pawn_key();
}

std::string Position::fen() const {
    std::string s;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            const Piece p = board_[make_square(f, r)];
            if (p == NO_PIECE) {
                ++empty;
                continue;
            }
            if (empty) s += char('0' + empty);
            empty = 0;
            s += PIECE_CHARS[p];
        }
        if (empty) s += char('0' + empty);
        if (r) s += '/';
    }
    s += side_ == WHITE ? " w " : " b ";
    const int cr = castling_rights();
    if (cr & WHITE_OO) s += 'K';
    if (cr & WHITE_OOO) s += 'Q';
    if (cr & BLACK_OO) s += 'k';
    if (cr & BLACK_OOO) s += 'q';
    if (!cr) s += '-';
    s += ' ' + square_to_string(ep_square());
    s += ' ' + std::to_string(halfmove_clock()) + ' ' + std::to_string(fullmove_);
    return s;
}

std::string Position::pretty() const {
    std::string s = "  +---+---+---+---+---+---+---+---+\n";
    for (int r = 7; r >= 0; --r) {
        s += char('1' + r);
        s += " |";
        for (int f = 0; f < 8; ++f) {
            const Piece p = board_[make_square(f, r)];
            s += ' ';
            s += p == NO_PIECE ? ' ' : PIECE_CHARS[p];
            s += " |";
        }
        s += "\n  +---+---+---+---+---+---+---+---+\n";
    }
    s += "    a   b   c   d   e   f   g   h\n\nFen: " + fen() + "\n";
    std::ostringstream key;
    key << std::hex << std::uppercase << this->key();
    s += "Key: " + key.str() + "\n";
    return s;
}

Key Position::compute_key() const {
    Key k = 0;
    for (int sq = 0; sq < 64; ++sq)
        if (board_[sq] != NO_PIECE) k ^= zobrist::piece_square[board_[sq]][sq];
    k ^= zobrist::castling[castling_rights()];
    if (ep_capturable(*this, ep_square())) k ^= zobrist::en_passant_file[file_of(ep_square())];
    if (side_ == BLACK) k ^= zobrist::side;
    return k;
}

Key Position::compute_pawn_key() const {
    Key k = 0;
    for (Bitboard b = pieces(PAWN); b;) {
        const Square sq = pop_lsb(b);
        k ^= zobrist::piece_square[board_[sq]][sq];
    }
    return k;
}

void Position::make_move(Move m) {
    StateInfo next = st();
    Key key = next.key ^ zobrist::side;
    if (ep_capturable(*this, next.ep_square)) key ^= zobrist::en_passant_file[file_of(next.ep_square)];
    next.ep_square = NO_SQUARE;
    next.captured = NO_PIECE;
    ++next.halfmove_clock;
    ++next.plies_from_null;

    const Color us = side_;
    const Square from = m.from(), to = m.to();
    const Piece pc = board_[from];

    if (m.is_castle()) {
        const bool king_side = m.flags() == KING_CASTLE;
        const Square rook_from = make_square(king_side ? 7 : 0, rank_of(from));
        const Square rook_to = make_square(king_side ? 5 : 3, rank_of(from));
        const Piece rook = board_[rook_from];
        move_piece(from, to);
        move_piece(rook_from, rook_to);
        key ^= zobrist::piece_square[pc][from] ^ zobrist::piece_square[pc][to];
        key ^= zobrist::piece_square[rook][rook_from] ^ zobrist::piece_square[rook][rook_to];
    } else {
        if (m.is_capture()) {
            const Square cap_sq = m.is_en_passant() ? Square(to ^ 8) : to;
            const Piece captured = board_[cap_sq];
            next.captured = captured;
            key ^= zobrist::piece_square[captured][cap_sq];
            if (type_of(captured) == PAWN) next.pawn_key ^= zobrist::piece_square[captured][cap_sq];
            remove_piece(cap_sq);
            next.halfmove_clock = 0;
        }

        move_piece(from, to);
        key ^= zobrist::piece_square[pc][from] ^ zobrist::piece_square[pc][to];

        if (type_of(pc) == PAWN) {
            next.halfmove_clock = 0;
            next.pawn_key ^= zobrist::piece_square[pc][from];
            if (m.is_promotion()) {
                const Piece promo = make_piece(us, m.promotion_type());
                remove_piece(to);
                put_piece(promo, to);
                key ^= zobrist::piece_square[pc][to] ^ zobrist::piece_square[promo][to];
            } else {
                next.pawn_key ^= zobrist::piece_square[pc][to];
                if (m.flags() == DOUBLE_PUSH) next.ep_square = Square((from + to) / 2);
            }
        }
    }

    const int new_castling = next.castling & castling_mask(from) & castling_mask(to);
    if (new_castling != next.castling) {
        key ^= zobrist::castling[next.castling] ^ zobrist::castling[new_castling];
        next.castling = new_castling;
    }

    side_ = ~us;
    if (us == BLACK) ++fullmove_;
    if (ep_capturable(*this, next.ep_square)) key ^= zobrist::en_passant_file[file_of(next.ep_square)];
    next.key = key;
    history_.push_back(next);
}

void Position::unmake_move(Move m) {
    const Piece captured = st().captured;
    history_.pop_back();
    side_ = ~side_;
    const Color us = side_;
    if (us == BLACK) --fullmove_;

    const Square from = m.from(), to = m.to();
    if (m.is_castle()) {
        const bool king_side = m.flags() == KING_CASTLE;
        const Square rook_from = make_square(king_side ? 7 : 0, rank_of(from));
        const Square rook_to = make_square(king_side ? 5 : 3, rank_of(from));
        move_piece(to, from);
        move_piece(rook_to, rook_from);
        return;
    }
    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    }
    move_piece(to, from);
    if (captured != NO_PIECE) put_piece(captured, m.is_en_passant() ? Square(to ^ 8) : to);
}

void Position::make_null_move() {
    StateInfo next = st();
    next.key ^= zobrist::side;
    if (ep_capturable(*this, next.ep_square)) next.key ^= zobrist::en_passant_file[file_of(next.ep_square)];
    next.ep_square = NO_SQUARE;
    next.captured = NO_PIECE;
    ++next.halfmove_clock;
    next.plies_from_null = 0;
    side_ = ~side_;
    history_.push_back(next);
}

void Position::unmake_null_move() {
    history_.pop_back();
    side_ = ~side_;
}

Move Position::parse_uci_move(std::string_view uci) const {
    for (Move m : legal_moves(*this))
        if (m.to_uci() == uci) return m;
    return Move::none();
}

Bitboard Position::attackers_to(int sq, Bitboard occ) const {
    return (attacks::pawn(BLACK, sq) & pieces(WHITE, PAWN)) |
           (attacks::pawn(WHITE, sq) & pieces(BLACK, PAWN)) |
           (attacks::knight(sq) & pieces(KNIGHT)) |
           (attacks::king(sq) & pieces(KING)) |
           (attacks::bishop(sq, occ) & (pieces(BISHOP) | pieces(QUEEN))) |
           (attacks::rook(sq, occ) & (pieces(ROOK) | pieces(QUEEN)));
}

bool Position::is_attacked(int sq, Color by) const {
    const Bitboard occ = occupied();
    return (attacks::pawn(~by, sq) & pieces(by, PAWN)) ||
           (attacks::knight(sq) & pieces(by, KNIGHT)) ||
           (attacks::king(sq) & pieces(by, KING)) ||
           (attacks::bishop(sq, occ) & (pieces(by, BISHOP) | pieces(by, QUEEN))) ||
           (attacks::rook(sq, occ) & (pieces(by, ROOK) | pieces(by, QUEEN)));
}

Bitboard Position::checkers() const {
    return attackers_to(king_square(side_), occupied()) & pieces(~side_);
}

Bitboard Position::pinned(Color c) const {
    const Square ksq = king_square(c);
    const Color them = ~c;
    Bitboard snipers = ((attacks::rook(ksq, 0) & (pieces(them, ROOK) | pieces(them, QUEEN))) |
                        (attacks::bishop(ksq, 0) & (pieces(them, BISHOP) | pieces(them, QUEEN))));
    const Bitboard occ = occupied();
    Bitboard result = 0;
    while (snipers) {
        const Square s = pop_lsb(snipers);
        const Bitboard blockers = attacks::between(ksq, s) & occ;
        if (blockers && !more_than_one(blockers)) result |= blockers & pieces(c);
    }
    return result;
}

bool Position::is_repetition() const {
    const int n = int(history_.size()) - 1;
    const int limit = std::min({st().halfmove_clock, st().plies_from_null, n});
    for (int i = 4; i <= limit; i += 2)
        if (history_[n - i].key == st().key) return true;
    return false;
}

bool Position::has_insufficient_material() const {
    if (pieces(PAWN) | pieces(ROOK) | pieces(QUEEN)) return false;
    return popcount(pieces(KNIGHT) | pieces(BISHOP)) <= 1;
}

bool Position::has_non_pawn_material(Color c) const {
    return pieces(c) & ~(pieces(PAWN) | pieces(KING));
}

bool Position::see_ge(Move m, int threshold) const {
    static constexpr int value[PIECE_TYPE_NB + 1] = {100, 320, 330, 500, 900, 20000, 0};
    if (m.is_castle() || m.is_en_passant() || m.is_promotion()) return 0 >= threshold;

    const Square from = m.from(), to = m.to();
    const Piece victim = board_[to];
    int swap = value[victim == NO_PIECE ? NO_PIECE_TYPE : type_of(victim)] - threshold;
    if (swap < 0) return false;
    swap = value[type_of(board_[from])] - swap;
    if (swap <= 0) return true;

    Bitboard occ = occupied() ^ square_bb(from) ^ square_bb(to);
    Color stm = color_of(board_[from]);
    Bitboard attackers = attackers_to(to, occ);
    const Bitboard diag = pieces(BISHOP) | pieces(QUEEN);
    const Bitboard ortho = pieces(ROOK) | pieces(QUEEN);
    int res = 1;

    for (;;) {
        stm = ~stm;
        attackers &= occ;
        const Bitboard stm_attackers = attackers & pieces(stm);
        if (!stm_attackers) break;
        res ^= 1;

        Bitboard bb;
        if ((bb = stm_attackers & pieces(PAWN))) {
            if ((swap = value[PAWN] - swap) < res) break;
            occ ^= square_bb(lsb(bb));
            attackers |= attacks::bishop(to, occ) & diag;
        } else if ((bb = stm_attackers & pieces(KNIGHT))) {
            if ((swap = value[KNIGHT] - swap) < res) break;
            occ ^= square_bb(lsb(bb));
        } else if ((bb = stm_attackers & pieces(BISHOP))) {
            if ((swap = value[BISHOP] - swap) < res) break;
            occ ^= square_bb(lsb(bb));
            attackers |= attacks::bishop(to, occ) & diag;
        } else if ((bb = stm_attackers & pieces(ROOK))) {
            if ((swap = value[ROOK] - swap) < res) break;
            occ ^= square_bb(lsb(bb));
            attackers |= attacks::rook(to, occ) & ortho;
        } else if ((bb = stm_attackers & pieces(QUEEN))) {
            if ((swap = value[QUEEN] - swap) < res) break;
            occ ^= square_bb(lsb(bb));
            attackers |= (attacks::bishop(to, occ) & diag) | (attacks::rook(to, occ) & ortho);
        } else {
            // Only the king is left: it may capture only if the opponent has no defenders left.
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
        }
    }
    return res;
}

}  // namespace chess
