#pragma once

#include <bit>
#include <string>

#include "types.h"

namespace chess {

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;
constexpr Bitboard RANK_1_BB = 0xFFULL;
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

constexpr Bitboard square_bb(int sq) { return 1ULL << sq; }
constexpr Bitboard file_bb(int file) { return FILE_A_BB << file; }
constexpr Bitboard rank_bb(int rank) { return RANK_1_BB << (8 * rank); }

inline int popcount(Bitboard b) { return std::popcount(b); }
inline Square lsb(Bitboard b) { return Square(std::countr_zero(b)); }
inline Square msb(Bitboard b) { return Square(63 - std::countl_zero(b)); }
inline Square pop_lsb(Bitboard& b) {
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}
constexpr bool more_than_one(Bitboard b) { return b & (b - 1); }

template <int Shift>
constexpr Bitboard shift(Bitboard b) {
    if constexpr (Shift == 8) return b << 8;
    else if constexpr (Shift == -8) return b >> 8;
    else if constexpr (Shift == 9) return (b & ~FILE_H_BB) << 9;
    else if constexpr (Shift == 7) return (b & ~FILE_A_BB) << 7;
    else if constexpr (Shift == -7) return (b & ~FILE_H_BB) >> 7;
    else if constexpr (Shift == -9) return (b & ~FILE_A_BB) >> 9;
    else static_assert(Shift == 8, "unsupported shift");
}

namespace attacks {

void init();

extern Bitboard pawn_table[COLOR_NB][SQUARE_NB];
extern Bitboard knight_table[SQUARE_NB];
extern Bitboard king_table[SQUARE_NB];
extern Bitboard between_table[SQUARE_NB][SQUARE_NB];
extern Bitboard line_table[SQUARE_NB][SQUARE_NB];

struct Magic {
    Bitboard mask;
    Bitboard magic;
    Bitboard* attacks;
    int shift;

    unsigned index(Bitboard occupied) const {
        return unsigned(((occupied & mask) * magic) >> shift);
    }
};

extern Magic bishop_magics[SQUARE_NB];
extern Magic rook_magics[SQUARE_NB];

inline Bitboard pawn(Color c, int sq) { return pawn_table[c][sq]; }
inline Bitboard knight(int sq) { return knight_table[sq]; }
inline Bitboard king(int sq) { return king_table[sq]; }
inline Bitboard bishop(int sq, Bitboard occupied) {
    const Magic& m = bishop_magics[sq];
    return m.attacks[m.index(occupied)];
}
inline Bitboard rook(int sq, Bitboard occupied) {
    const Magic& m = rook_magics[sq];
    return m.attacks[m.index(occupied)];
}
inline Bitboard queen(int sq, Bitboard occupied) { return bishop(sq, occupied) | rook(sq, occupied); }

// Squares strictly between a and b if they share a rank, file or diagonal, otherwise 0.
inline Bitboard between(int a, int b) { return between_table[a][b]; }
// The full line (edge to edge) through a and b if aligned, otherwise 0.
inline Bitboard line(int a, int b) { return line_table[a][b]; }
inline bool aligned(int a, int b, int c) { return line_table[a][b] & square_bb(c); }

}  // namespace attacks

std::string bitboard_to_string(Bitboard b);

}  // namespace chess
