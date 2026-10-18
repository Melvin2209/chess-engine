#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>

#include "eval.h"
#include "movegen.h"
#include "position.h"

using namespace chess;

namespace {

// Flips the board vertically and swaps colors: the evaluation must be identical
// for the side to move.
std::string mirror_fen(const std::string& fen) {
    std::istringstream in(fen);
    std::string board, side, castling, ep, half, full;
    in >> board >> side >> castling >> ep >> half >> full;

    std::vector<std::string> ranks;
    std::stringstream bs(board);
    for (std::string r; std::getline(bs, r, '/');) ranks.push_back(r);
    std::reverse(ranks.begin(), ranks.end());
    std::string out;
    for (std::size_t i = 0; i < ranks.size(); ++i) {
        for (char c : ranks[i]) out += std::isalpha(c) ? char(std::isupper(c) ? std::tolower(c) : std::toupper(c)) : c;
        if (i + 1 < ranks.size()) out += '/';
    }
    std::string cr;
    for (char c : castling) cr += c == '-' ? c : char(std::isupper(c) ? std::tolower(c) : std::toupper(c));
    std::sort(cr.begin(), cr.end());  // uppercase sorts before lowercase: "KQkq" order
    if (ep != "-") ep[1] = ep[1] == '3' ? '6' : '3';
    return out + (side == "w" ? " b " : " w ") + cr + ' ' + ep + ' ' + half + ' ' + full;
}

}  // namespace

TEST(Eval, StartposIsBalanced) {
    // Only the tempo bonus remains in a symmetric position.
    const Score s = evaluate(Position());
    EXPECT_GT(s, 0);
    EXPECT_LT(s, 30);
}

TEST(Eval, SymmetricUnderColorMirror) {
    std::mt19937 rng(7);
    for (int game = 0; game < 50; ++game) {
        Position pos;
        for (int ply = 0; ply < 80; ++ply) {
            const MoveList moves = legal_moves(pos);
            if (moves.empty()) break;
            pos.make_move(moves[int(rng() % unsigned(moves.size()))]);
            const Position mirrored(mirror_fen(pos.fen()));
            ASSERT_EQ(evaluate(pos), evaluate(mirrored)) << pos.fen() << " vs " << mirrored.fen();
        }
    }
}

TEST(Eval, MaterialAdvantageIsPositive) {
    // White is a queen up.
    EXPECT_GT(evaluate(Position("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")), 700);
    EXPECT_LT(evaluate(Position("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1")), -700);
}

TEST(Eval, PassedPawnIsRewarded) {
    // Same material in both: the black a7 pawn leaves d5 passed, the black e7 pawn does not.
    const Score passed = evaluate(Position("4k3/p7/8/3P4/8/8/8/4K3 w - - 0 1"));
    const Score not_passed = evaluate(Position("4k3/4p3/8/3P4/8/8/8/4K3 w - - 0 1"));
    EXPECT_GT(passed, not_passed + 20);
}

TEST(Eval, DoubledAndIsolatedPawnsArePenalized) {
    const Score healthy = evaluate(Position("4k3/8/8/8/8/8/3PP3/4K3 w - - 0 1"));
    const Score doubled = evaluate(Position("4k3/8/8/8/8/4P3/4P3/4K3 w - - 0 1"));
    EXPECT_GT(healthy, doubled);
}

TEST(Eval, BishopPairBonus) {
    const Score pair = evaluate(Position("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1"));
    const Score knight_bishop = evaluate(Position("4k3/8/8/8/8/8/8/2B1KN2 w - - 0 1"));
    EXPECT_GT(pair, knight_bishop);
}
