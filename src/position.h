#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "bitboard.h"
#include "types.h"

namespace chess {

namespace zobrist {
extern Key piece_square[PIECE_NB][SQUARE_NB];
extern Key castling[16];
extern Key en_passant_file[8];
extern Key side;
void init();
}  // namespace zobrist

// Everything needed to undo a move that cannot be recomputed from the move itself.
struct StateInfo {
    Key key;
    Key pawn_key;
    int castling;
    Square ep_square;
    int halfmove_clock;
    int plies_from_null;
    Piece captured;
};

class Position {
public:
    static constexpr std::string_view START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Position();
    explicit Position(std::string_view fen);

    // Throws std::invalid_argument on malformed FEN.
    void set_fen(std::string_view fen);
    std::string fen() const;
    std::string pretty() const;

    void make_move(Move m);
    void unmake_move(Move m);
    void make_null_move();
    void unmake_null_move();

    // Parse a move in UCI notation (e.g. "e7e8q") against the legal moves of this position.
    Move parse_uci_move(std::string_view uci) const;

    Color side_to_move() const { return side_; }
    Piece piece_on(int sq) const { return board_[sq]; }
    Bitboard pieces(Color c, PieceType pt) const { return by_type_[pt] & by_color_[c]; }
    Bitboard pieces(PieceType pt) const { return by_type_[pt]; }
    Bitboard pieces(Color c) const { return by_color_[c]; }
    Bitboard occupied() const { return by_color_[WHITE] | by_color_[BLACK]; }
    Square king_square(Color c) const { return lsb(pieces(c, KING)); }
    int castling_rights() const { return st().castling; }
    Square ep_square() const { return st().ep_square; }
    int halfmove_clock() const { return st().halfmove_clock; }
    int fullmove_number() const { return fullmove_; }
    Key key() const { return st().key; }
    Key pawn_key() const { return st().pawn_key; }
    int game_ply() const { return int(history_.size()) - 1; }
    // 0 right after a null move; used to forbid two null moves in a row.
    int plies_from_null() const { return st().plies_from_null; }

    // All pieces of either color attacking `sq`, given an occupancy (allows x-ray style queries).
    Bitboard attackers_to(int sq, Bitboard occupied) const;
    bool is_attacked(int sq, Color by) const;
    bool in_check() const { return checkers() != 0; }
    Bitboard checkers() const;
    // Pieces of `c` that are absolutely pinned to their own king.
    Bitboard pinned(Color c) const;

    bool is_repetition() const;
    bool is_fifty_move_draw() const { return halfmove_clock() >= 100; }
    bool has_insufficient_material() const;
    // True when the side to move has any piece other than pawns and king (null-move safety).
    bool has_non_pawn_material(Color c) const;

    // Static exchange evaluation: does the capture sequence on m.to() gain at least `threshold`?
    bool see_ge(Move m, int threshold) const;

    Key compute_key() const;
    Key compute_pawn_key() const;

private:
    const StateInfo& st() const { return history_.back(); }
    StateInfo& st() { return history_.back(); }

    void clear();
    void put_piece(Piece p, int sq);
    void remove_piece(int sq);
    void move_piece(int from, int to);

    Piece board_[SQUARE_NB];
    Bitboard by_type_[PIECE_TYPE_NB];
    Bitboard by_color_[COLOR_NB];
    Color side_ = WHITE;
    int fullmove_ = 1;
    std::vector<StateInfo> history_;
};

}  // namespace chess
