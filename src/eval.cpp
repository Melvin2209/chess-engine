#include "eval.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace chess {

namespace {

using eval::Weights;

// Piece-square tables, written from white's point of view with rank 8 on the first line
// (so they read like a board diagram). A white piece on `sq` uses index `flip(sq)`,
// a black piece uses `sq` directly. Values are adapted from Tomasz Michniewski's
// "Simplified Evaluation Function", with separate endgame tables for pawns and king.
using Table = std::array<int, 64>;

constexpr Table PAWN_MG = {
      0,   0,   0,   0,   0,   0,   0,   0,
     30,  30,  30,  30,  30,  30,  30,  30,
     10,  10,  20,  30,  30,  20,  10,  10,
      5,   5,  10,  25,  25,  10,   5,   5,
      0,   0,   0,  20,  20,   0,   0,   0,
      5,  -5, -10,   0,   0, -10,  -5,   5,
      5,  10,  10, -20, -20,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0};

constexpr Table PAWN_EG = {
      0,   0,   0,   0,   0,   0,   0,   0,
     50,  50,  50,  50,  50,  50,  50,  50,
     30,  30,  30,  30,  30,  30,  30,  30,
     15,  15,  15,  15,  15,  15,  15,  15,
      5,   5,   5,   5,   5,   5,   5,   5,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0};

constexpr Table KNIGHT_PST = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50};

constexpr Table BISHOP_PST = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

constexpr Table ROOK_PST = {
      0,   0,   0,   0,   0,   0,   0,   0,
      5,  10,  10,  10,  10,  10,  10,   5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      0,   0,   0,   5,   5,   0,   0,   0};

constexpr Table QUEEN_PST = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20};

constexpr Table KING_MG = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20};

constexpr Table KING_EG = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50};

constexpr const Table* PST_MG[PIECE_TYPE_NB] = {&PAWN_MG, &KNIGHT_PST, &BISHOP_PST, &ROOK_PST, &QUEEN_PST, &KING_MG};
constexpr const Table* PST_EG[PIECE_TYPE_NB] = {&PAWN_EG, &KNIGHT_PST, &BISHOP_PST, &ROOK_PST, &QUEEN_PST, &KING_EG};

constexpr Weights MATERIAL[PIECE_TYPE_NB] = {{100, 120}, {320, 300}, {330, 315}, {500, 530}, {900, 950}, {0, 0}};

// Pawn structure terms.
constexpr Weights DOUBLED_PAWN = {-10, -20};
constexpr Weights ISOLATED_PAWN = {-12, -16};
// Passed pawn bonus indexed by relative rank (rank 2 = index 1 ... rank 7 = index 6).
constexpr Weights PASSED_PAWN[8] = {{0, 0}, {5, 10}, {5, 15}, {10, 25}, {20, 45}, {35, 75}, {55, 110}, {0, 0}};
constexpr Weights BISHOP_PAIR = {30, 50};
constexpr int TEMPO = 10;

struct Masks {
    Bitboard passed[COLOR_NB][SQUARE_NB];  // squares in front on the same and adjacent files
    Bitboard adjacent_files[8];

    Masks() {
        for (int f = 0; f < 8; ++f)
            adjacent_files[f] = (f > 0 ? file_bb(f - 1) : 0) | (f < 7 ? file_bb(f + 1) : 0);
        for (int sq = 0; sq < 64; ++sq) {
            const Bitboard files = adjacent_files[file_of(sq)] | file_bb(file_of(sq));
            Bitboard ahead_white = 0, ahead_black = 0;
            for (int r = rank_of(sq) + 1; r < 8; ++r) ahead_white |= rank_bb(r);
            for (int r = rank_of(sq) - 1; r >= 0; --r) ahead_black |= rank_bb(r);
            passed[WHITE][sq] = files & ahead_white;
            passed[BLACK][sq] = files & ahead_black;
        }
    }
};

const Masks masks;

struct Accumulator {
    int mg = 0;
    int eg = 0;
    void add(Weights w, int count = 1) {
        mg += w.mg * count;
        eg += w.eg * count;
    }
};

struct Breakdown {
    Accumulator material[COLOR_NB];
    Accumulator pst[COLOR_NB];
    Accumulator pawns[COLOR_NB];
    Accumulator imbalance[COLOR_NB];
    int phase = 0;
};

void evaluate_pawns(const Position& pos, Color c, Accumulator& acc) {
    const Bitboard ours = pos.pieces(c, PAWN);
    const Bitboard theirs = pos.pieces(~c, PAWN);

    for (int f = 0; f < 8; ++f) {
        const int on_file = popcount(ours & file_bb(f));
        if (on_file > 1) acc.add(DOUBLED_PAWN, on_file - 1);
    }

    for (Bitboard b = ours; b;) {
        const Square sq = pop_lsb(b);
        if (!(ours & masks.adjacent_files[file_of(sq)])) acc.add(ISOLATED_PAWN);
        // Passed: no enemy pawn ahead on the same or adjacent files, and not blocked by a friendly pawn.
        const Bitboard front_same_file = masks.passed[c][sq] & file_bb(file_of(sq));
        if (!(masks.passed[c][sq] & theirs) && !(front_same_file & ours))
            acc.add(PASSED_PAWN[relative_rank(c, sq)]);
    }
}

Breakdown compute(const Position& pos) {
    Breakdown bd;
    for (Color c : {WHITE, BLACK}) {
        for (int pt = PAWN; pt <= KING; ++pt) {
            for (Bitboard b = pos.pieces(c, PieceType(pt)); b;) {
                const Square sq = pop_lsb(b);
                const int idx = c == WHITE ? flip(sq) : sq;
                bd.material[c].add(MATERIAL[pt]);
                bd.pst[c].add({(*PST_MG[pt])[idx], (*PST_EG[pt])[idx]});
                bd.phase += eval::PHASE_WEIGHT[pt];
            }
        }
        evaluate_pawns(pos, c, bd.pawns[c]);
        if (more_than_one(pos.pieces(c, BISHOP))) bd.imbalance[c].add(BISHOP_PAIR);
    }
    return bd;
}

int taper(int mg, int eg, int phase) {
    phase = std::min(phase, eval::MAX_PHASE);
    return (mg * phase + eg * (eval::MAX_PHASE - phase)) / eval::MAX_PHASE;
}

}  // namespace

Score evaluate(const Position& pos) {
    const Breakdown bd = compute(pos);
    int mg = 0, eg = 0;
    for (const Accumulator* terms : {bd.material, bd.pst, bd.pawns, bd.imbalance}) {
        mg += terms[WHITE].mg - terms[BLACK].mg;
        eg += terms[WHITE].eg - terms[BLACK].eg;
    }
    const int white_score = taper(mg, eg, bd.phase);
    return (pos.side_to_move() == WHITE ? white_score : -white_score) + TEMPO;
}

std::string eval_trace(const Position& pos) {
    const Breakdown bd = compute(pos);
    std::string out = "      Term |    White   |    Black   |    Total\n"
                      "           |   MG    EG |   MG    EG |   MG    EG\n"
                      " ----------+------------+------------+-----------\n";
    char line[128];
    auto row = [&](const char* name, const Accumulator* t) {
        std::snprintf(line, sizeof line, " %9s | %4d  %4d | %4d  %4d | %4d  %4d\n", name, t[WHITE].mg, t[WHITE].eg,
                      t[BLACK].mg, t[BLACK].eg, t[WHITE].mg - t[BLACK].mg, t[WHITE].eg - t[BLACK].eg);
        out += line;
    };
    row("Material", bd.material);
    row("PST", bd.pst);
    row("Pawns", bd.pawns);
    row("Imbalance", bd.imbalance);
    std::snprintf(line, sizeof line, "\nPhase: %d/%d\nFinal evaluation: %+d cp (white's point of view)\n",
                  std::min(bd.phase, eval::MAX_PHASE), eval::MAX_PHASE,
                  pos.side_to_move() == WHITE ? evaluate(pos) : -evaluate(pos));
    out += line;
    return out;
}

}  // namespace chess
