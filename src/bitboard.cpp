#include "bitboard.h"

#include <array>
#include <vector>

namespace chess {

namespace attacks {

Bitboard pawn_table[COLOR_NB][SQUARE_NB];
Bitboard knight_table[SQUARE_NB];
Bitboard king_table[SQUARE_NB];
Bitboard between_table[SQUARE_NB][SQUARE_NB];
Bitboard line_table[SQUARE_NB][SQUARE_NB];
Magic bishop_magics[SQUARE_NB];
Magic rook_magics[SQUARE_NB];

namespace {

// 0x1480 = 5248 entries for bishops, 0x19000 = 102400 for rooks (sum of 2^bits over all squares).
std::vector<Bitboard> bishop_storage(0x1480);
std::vector<Bitboard> rook_storage(0x19000);

constexpr std::array<std::pair<int, int>, 4> ROOK_DIRS{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
constexpr std::array<std::pair<int, int>, 4> BISHOP_DIRS{{{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

bool on_board(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

Bitboard sliding_attacks(int sq, Bitboard occupied, const std::array<std::pair<int, int>, 4>& dirs) {
    Bitboard result = 0;
    for (auto [df, dr] : dirs) {
        int f = file_of(sq) + df;
        int r = rank_of(sq) + dr;
        while (on_board(f, r)) {
            const int s = make_square(f, r);
            result |= square_bb(s);
            if (occupied & square_bb(s)) break;
            f += df;
            r += dr;
        }
    }
    return result;
}

// xorshift64* generator: deterministic so that magic search gives the same tables on every run.
struct Prng {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ULL;
    }
    // Magics need few set bits to work well.
    std::uint64_t sparse() { return next() & next() & next(); }
};

void init_magics(Magic* magics, std::vector<Bitboard>& storage, const std::array<std::pair<int, int>, 4>& dirs) {
    Prng rng{0x9E3779B97F4A7C15ULL};
    Bitboard* next_table = storage.data();
    std::vector<Bitboard> occupancy(4096), reference(4096);
    std::vector<int> epoch(4096, 0);
    int attempt = 0;

    for (int sq = 0; sq < 64; ++sq) {
        // Edges never block further sliding, so they are excluded from the relevant mask
        // unless the piece itself sits on that edge.
        const Bitboard edges = ((RANK_1_BB | RANK_8_BB) & ~rank_bb(rank_of(sq))) |
                               ((FILE_A_BB | FILE_H_BB) & ~file_bb(file_of(sq)));
        Magic& m = magics[sq];
        m.mask = sliding_attacks(sq, 0, dirs) & ~edges;
        const int bits = popcount(m.mask);
        m.shift = 64 - bits;
        m.attacks = next_table;

        // Enumerate all subsets of the mask (Carry-Rippler trick).
        int size = 0;
        Bitboard b = 0;
        do {
            occupancy[size] = b;
            reference[size] = sliding_attacks(sq, b, dirs);
            ++size;
            b = (b - m.mask) & m.mask;
        } while (b);

        for (;;) {
            do {
                m.magic = rng.sparse();
            } while (popcount((m.mask * m.magic) >> 56) < 6);

            ++attempt;
            bool ok = true;
            for (int i = 0; i < size && ok; ++i) {
                const unsigned idx = m.index(occupancy[i]);
                if (epoch[idx] < attempt) {
                    epoch[idx] = attempt;
                    m.attacks[idx] = reference[i];
                } else if (m.attacks[idx] != reference[i]) {
                    ok = false;
                }
            }
            if (ok) break;
        }
        next_table += size;
    }
}

}  // namespace

void init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    for (int sq = 0; sq < 64; ++sq) {
        const Bitboard b = square_bb(sq);
        pawn_table[WHITE][sq] = shift<7>(b) | shift<9>(b);
        pawn_table[BLACK][sq] = shift<-7>(b) | shift<-9>(b);

        knight_table[sq] = 0;
        king_table[sq] = 0;
        constexpr int knight_steps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
        constexpr int king_steps[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        for (int i = 0; i < 8; ++i) {
            const int kf = file_of(sq) + knight_steps[i][0], kr = rank_of(sq) + knight_steps[i][1];
            if (on_board(kf, kr)) knight_table[sq] |= square_bb(make_square(kf, kr));
            const int gf = file_of(sq) + king_steps[i][0], gr = rank_of(sq) + king_steps[i][1];
            if (on_board(gf, gr)) king_table[sq] |= square_bb(make_square(gf, gr));
        }
    }

    init_magics(bishop_magics, bishop_storage, BISHOP_DIRS);
    init_magics(rook_magics, rook_storage, ROOK_DIRS);

    for (int a = 0; a < 64; ++a) {
        for (int b = 0; b < 64; ++b) {
            between_table[a][b] = 0;
            line_table[a][b] = 0;
            if (a == b) continue;
            for (auto slider : {BISHOP, ROOK}) {
                const Bitboard from_a = slider == BISHOP ? bishop(a, 0) : rook(a, 0);
                if (from_a & square_bb(b)) {
                    const Bitboard from_b = slider == BISHOP ? bishop(b, 0) : rook(b, 0);
                    line_table[a][b] = (from_a & from_b) | square_bb(a) | square_bb(b);
                    between_table[a][b] = slider == BISHOP
                                              ? bishop(a, square_bb(b)) & bishop(b, square_bb(a))
                                              : rook(a, square_bb(b)) & rook(b, square_bb(a));
                }
            }
        }
    }
}

}  // namespace attacks

std::string bitboard_to_string(Bitboard b) {
    std::string s;
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            s += (b & square_bb(make_square(f, r))) ? 'X' : '.';
            s += f < 7 ? ' ' : '\n';
        }
    }
    return s;
}

}  // namespace chess
