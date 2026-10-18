#include "movegen.h"

#include <algorithm>

namespace chess {

bool MoveList::contains(Move m) const { return std::find(begin(), end(), m) != end(); }

namespace {

struct GenContext {
    const Position& pos;
    MoveList& list;
    Color us;
    Color them;
    Square ksq;
    Bitboard occ;
    Bitboard pinned;
    Bitboard evasion;       // squares that resolve a single check (everything when not in check)
    Bitboard capture_mask;  // destinations of capturing moves
    Bitboard quiet_mask;    // destinations of non-capturing moves
};

// A pinned piece may only move along the line joining it to its king.
inline bool pin_ok(const GenContext& ctx, Square from, Square to) {
    return !(ctx.pinned & square_bb(from)) || attacks::aligned(ctx.ksq, from, to);
}

template <GenType Type>
void add_promotions(MoveList& list, Square from, Square to, bool capture) {
    const int base = capture ? PROMO_CAPTURE_KNIGHT : PROMO_KNIGHT;
    list.push_back(Move(from, to, base + 3));  // queen
    if constexpr (Type == GenType::ALL) {
        list.push_back(Move(from, to, base + 0));
        list.push_back(Move(from, to, base + 1));
        list.push_back(Move(from, to, base + 2));
    } else if (capture) {
        // Under-promotions that capture are still captures, so they belong to the noisy set.
        list.push_back(Move(from, to, base + 0));
        list.push_back(Move(from, to, base + 1));
        list.push_back(Move(from, to, base + 2));
    }
}

template <GenType Type>
void generate_pawn_moves(const GenContext& ctx) {
    const Position& pos = ctx.pos;
    const Color us = ctx.us;
    const int up = us == WHITE ? 8 : -8;
    const Bitboard pawns = pos.pieces(us, PAWN);
    const Bitboard seventh = us == WHITE ? RANK_7_BB : rank_bb(1);
    const Bitboard third = us == WHITE ? rank_bb(2) : rank_bb(5);
    const Bitboard empty = ~ctx.occ;
    const Bitboard enemies = pos.pieces(ctx.them);

    auto push = [us](Bitboard b) { return us == WHITE ? shift<8>(b) : shift<-8>(b); };
    auto cap_left = [us](Bitboard b) { return us == WHITE ? shift<7>(b) : shift<-9>(b); };
    auto cap_right = [us](Bitboard b) { return us == WHITE ? shift<9>(b) : shift<-7>(b); };
    const int left_delta = us == WHITE ? 7 : -9;
    const int right_delta = us == WHITE ? 9 : -7;

    const Bitboard promoting = pawns & seventh;
    const Bitboard normal = pawns & ~seventh;

    // Single and double pushes (quiet, not promoting).
    if constexpr (Type == GenType::ALL) {
        Bitboard single = push(normal) & empty;
        Bitboard dbl = push(single & third) & empty & ctx.quiet_mask;
        single &= ctx.quiet_mask;
        while (single) {
            const Square to = pop_lsb(single);
            const Square from = Square(to - up);
            if (pin_ok(ctx, from, to)) ctx.list.push_back(Move(from, to, QUIET));
        }
        while (dbl) {
            const Square to = pop_lsb(dbl);
            const Square from = Square(to - 2 * up);
            if (pin_ok(ctx, from, to)) ctx.list.push_back(Move(from, to, DOUBLE_PUSH));
        }
    }

    // Promotions.
    if (promoting) {
        // Quiet queen promotions are part of the noisy set, so this uses the evasion mask directly.
        Bitboard b = push(promoting) & empty & ctx.evasion;
        while (b) {
            const Square to = pop_lsb(b);
            const Square from = Square(to - up);
            if (pin_ok(ctx, from, to)) add_promotions<Type>(ctx.list, from, to, false);
        }
        Bitboard l = cap_left(promoting) & enemies & ctx.capture_mask;
        while (l) {
            const Square to = pop_lsb(l);
            const Square from = Square(to - left_delta);
            if (pin_ok(ctx, from, to)) add_promotions<Type>(ctx.list, from, to, true);
        }
        Bitboard r = cap_right(promoting) & enemies & ctx.capture_mask;
        while (r) {
            const Square to = pop_lsb(r);
            const Square from = Square(to - right_delta);
            if (pin_ok(ctx, from, to)) add_promotions<Type>(ctx.list, from, to, true);
        }
    }

    // Regular captures.
    Bitboard l = cap_left(normal) & enemies & ctx.capture_mask;
    while (l) {
        const Square to = pop_lsb(l);
        const Square from = Square(to - left_delta);
        if (pin_ok(ctx, from, to)) ctx.list.push_back(Move(from, to, CAPTURE));
    }
    Bitboard r = cap_right(normal) & enemies & ctx.capture_mask;
    while (r) {
        const Square to = pop_lsb(r);
        const Square from = Square(to - right_delta);
        if (pin_ok(ctx, from, to)) ctx.list.push_back(Move(from, to, CAPTURE));
    }

    // En passant: verified by simulating the resulting occupancy, which handles the
    // rare horizontal pin where both pawns leave the king's rank at once.
    const Square ep = pos.ep_square();
    if (ep != NO_SQUARE) {
        const Square cap_sq = Square(ep - up);
        Bitboard candidates = attacks::pawn(ctx.them, ep) & normal;
        while (candidates) {
            const Square from = pop_lsb(candidates);
            const Bitboard occ_after = (ctx.occ ^ square_bb(from) ^ square_bb(cap_sq)) | square_bb(ep);
            const Bitboard attackers =
                pos.attackers_to(ctx.ksq, occ_after) & pos.pieces(ctx.them) & ~square_bb(cap_sq);
            if (!attackers) ctx.list.push_back(Move(from, ep, EP_CAPTURE));
        }
    }
}

template <PieceType Pt>
void generate_piece_moves(const GenContext& ctx) {
    Bitboard pieces = ctx.pos.pieces(ctx.us, Pt);
    while (pieces) {
        const Square from = pop_lsb(pieces);
        Bitboard targets;
        if constexpr (Pt == KNIGHT) targets = attacks::knight(from);
        else if constexpr (Pt == BISHOP) targets = attacks::bishop(from, ctx.occ);
        else if constexpr (Pt == ROOK) targets = attacks::rook(from, ctx.occ);
        else targets = attacks::queen(from, ctx.occ);

        if (ctx.pinned & square_bb(from)) targets &= attacks::line(ctx.ksq, from);

        Bitboard caps = targets & ctx.capture_mask;
        while (caps) ctx.list.push_back(Move(from, pop_lsb(caps), CAPTURE));
        Bitboard quiets = targets & ctx.quiet_mask;
        while (quiets) ctx.list.push_back(Move(from, pop_lsb(quiets), QUIET));
    }
}

template <GenType Type>
void generate_king_moves(const GenContext& ctx, bool in_check) {
    const Position& pos = ctx.pos;
    const Bitboard occ_without_king = ctx.occ ^ square_bb(ctx.ksq);
    const Bitboard enemies = pos.pieces(ctx.them);
    Bitboard targets = attacks::king(ctx.ksq) & ~pos.pieces(ctx.us);
    if constexpr (Type == GenType::NOISY) targets &= enemies;

    while (targets) {
        const Square to = pop_lsb(targets);
        // The king itself is removed from the occupancy so sliders "see through" it.
        if (pos.attackers_to(to, occ_without_king) & enemies) continue;
        ctx.list.push_back(Move(ctx.ksq, to, (enemies & square_bb(to)) ? CAPTURE : QUIET));
    }

    if constexpr (Type == GenType::ALL) {
        if (in_check) return;
        const int rights = pos.castling_rights();
        const Color us = ctx.us;
        const int oo = us == WHITE ? WHITE_OO : BLACK_OO;
        const int ooo = us == WHITE ? WHITE_OOO : BLACK_OOO;
        const int rank = us == WHITE ? 0 : 7;
        const Square e = make_square(4, rank);

        if ((rights & oo) && ctx.ksq == e) {
            const Square f = make_square(5, rank), g = make_square(6, rank);
            if (!(ctx.occ & (square_bb(f) | square_bb(g))) && !pos.is_attacked(f, ctx.them) &&
                !pos.is_attacked(g, ctx.them))
                ctx.list.push_back(Move(e, g, KING_CASTLE));
        }
        if ((rights & ooo) && ctx.ksq == e) {
            const Square d = make_square(3, rank), c = make_square(2, rank), b = make_square(1, rank);
            if (!(ctx.occ & (square_bb(d) | square_bb(c) | square_bb(b))) && !pos.is_attacked(d, ctx.them) &&
                !pos.is_attacked(c, ctx.them))
                ctx.list.push_back(Move(e, c, QUEEN_CASTLE));
        }
    }
}

}  // namespace

template <GenType Type>
void generate(const Position& pos, MoveList& list) {
    const Color us = pos.side_to_move();
    const Color them = ~us;
    const Square ksq = pos.king_square(us);
    const Bitboard checkers = pos.checkers();

    GenContext ctx{pos, list, us, them, ksq, pos.occupied(), pos.pinned(us), 0, 0, 0};

    if (!more_than_one(checkers)) {
        // When in single check, non-king moves must capture the checker or block the line.
        ctx.evasion = checkers ? (attacks::between(ksq, lsb(checkers)) | checkers) : ~0ULL;
        ctx.capture_mask = pos.pieces(them) & ctx.evasion;
        ctx.quiet_mask = Type == GenType::ALL ? (~ctx.occ & ctx.evasion) : 0;

        generate_pawn_moves<Type>(ctx);
        generate_piece_moves<KNIGHT>(ctx);
        generate_piece_moves<BISHOP>(ctx);
        generate_piece_moves<ROOK>(ctx);
        generate_piece_moves<QUEEN>(ctx);
    }
    // In double check only the king can move.
    generate_king_moves<Type>(ctx, checkers != 0);
}

template void generate<GenType::ALL>(const Position&, MoveList&);
template void generate<GenType::NOISY>(const Position&, MoveList&);

std::uint64_t perft(Position& pos, int depth) {
    MoveList list;
    generate<GenType::ALL>(pos, list);
    if (depth <= 1) return depth == 1 ? list.size() : 1;

    std::uint64_t nodes = 0;
    for (Move m : list) {
        pos.make_move(m);
        nodes += perft(pos, depth - 1);
        pos.unmake_move(m);
    }
    return nodes;
}

}  // namespace chess
