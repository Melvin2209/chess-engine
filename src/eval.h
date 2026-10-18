#pragma once

#include "position.h"

namespace chess {

namespace eval {

// Middlegame / endgame pair, blended according to the remaining material (tapered eval).
struct Weights {
    int mg;
    int eg;
};

// Simple values used for move ordering (MVV-LVA) and pruning margins.
constexpr int SIMPLE_VALUE[PIECE_TYPE_NB + 1] = {100, 320, 330, 500, 900, 0, 0};

// Game phase: 24 at the start (minor = 1, rook = 2, queen = 4), 0 with kings and pawns only.
constexpr int PHASE_WEIGHT[PIECE_TYPE_NB] = {0, 1, 1, 2, 4, 0};
constexpr int MAX_PHASE = 24;

}  // namespace eval

// Static evaluation in centipawns from the side to move's point of view.
Score evaluate(const Position& pos);

// Detailed breakdown (white's point of view) printed by the UCI "eval" command.
std::string eval_trace(const Position& pos);

}  // namespace chess
