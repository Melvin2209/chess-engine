#pragma once

#include <cstdint>
#include <string>

namespace chess {

using Bitboard = std::uint64_t;
using Key = std::uint64_t;

enum Color : int { WHITE, BLACK, COLOR_NB = 2 };

constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_TYPE_NB, NO_PIECE_TYPE = PIECE_TYPE_NB };

// Pieces are encoded as color * 6 + type, so W_PAWN..W_KING = 0..5 and B_PAWN..B_KING = 6..11.
enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB, NO_PIECE = PIECE_NB
};

constexpr Piece make_piece(Color c, PieceType pt) { return Piece(c * 6 + pt); }
constexpr Color color_of(Piece p) { return Color(p / 6); }
constexpr PieceType type_of(Piece p) { return PieceType(p % 6); }

// Squares are numbered a1 = 0, b1 = 1, ..., h8 = 63.
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_NB, NO_SQUARE = SQUARE_NB
};

constexpr int file_of(int sq) { return sq & 7; }
constexpr int rank_of(int sq) { return sq >> 3; }
constexpr Square make_square(int file, int rank) { return Square(rank * 8 + file); }
// Mirror a square vertically (a1 <-> a8); used to view the board from black's side.
constexpr Square flip(int sq) { return Square(sq ^ 56); }
// Rank of a square as seen by the given side (0 = own back rank).
constexpr int relative_rank(Color c, int sq) { return c == WHITE ? rank_of(sq) : 7 - rank_of(sq); }

enum CastlingRight : int {
    NO_CASTLING = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    ALL_CASTLING = 15
};

// 16-bit move: bits 0-5 from, 6-11 to, 12-15 flags.
// Flag layout follows the classic "from-to + 4 bit flags" scheme:
//   bit 2 (value 4) = capture, bit 3 (value 8) = promotion.
enum MoveFlag : int {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    PROMO_KNIGHT = 8,
    PROMO_BISHOP = 9,
    PROMO_ROOK = 10,
    PROMO_QUEEN = 11,
    PROMO_CAPTURE_KNIGHT = 12,
    PROMO_CAPTURE_BISHOP = 13,
    PROMO_CAPTURE_ROOK = 14,
    PROMO_CAPTURE_QUEEN = 15
};

class Move {
public:
    constexpr Move() = default;
    constexpr Move(int from, int to, int flags) : data_(std::uint16_t(from | (to << 6) | (flags << 12))) {}

    static constexpr Move none() { return Move(); }

    constexpr Square from() const { return Square(data_ & 63); }
    constexpr Square to() const { return Square((data_ >> 6) & 63); }
    constexpr int flags() const { return data_ >> 12; }

    constexpr bool is_capture() const { return flags() & 4; }
    constexpr bool is_promotion() const { return flags() & 8; }
    constexpr bool is_castle() const { return flags() == KING_CASTLE || flags() == QUEEN_CASTLE; }
    constexpr bool is_en_passant() const { return flags() == EP_CAPTURE; }
    constexpr bool is_quiet() const { return !is_capture() && !is_promotion(); }
    constexpr PieceType promotion_type() const { return PieceType(KNIGHT + (flags() & 3)); }

    constexpr std::uint16_t raw() const { return data_; }
    static constexpr Move from_raw(std::uint16_t raw) {
        Move m;
        m.data_ = raw;
        return m;
    }

    constexpr bool operator==(const Move&) const = default;
    explicit constexpr operator bool() const { return data_ != 0; }

    // Long algebraic notation used by UCI, e.g. "e2e4", "e7e8q".
    std::string to_uci() const;

private:
    std::uint16_t data_ = 0;
};

std::string square_to_string(int sq);

using Score = int;

constexpr Score SCORE_INF = 32000;
constexpr Score SCORE_MATE = 31000;
constexpr Score SCORE_MATE_IN_MAX = SCORE_MATE - 256;
constexpr Score SCORE_NONE = 32001;
constexpr int MAX_PLY = 128;

}  // namespace chess
