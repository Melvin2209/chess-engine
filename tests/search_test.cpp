#include <gtest/gtest.h>

#include <chrono>

#include "movegen.h"
#include "search.h"

using namespace chess;

namespace {

SearchResult search_depth(const char* fen, int depth) {
    TranspositionTable tt(8);
    Searcher searcher(tt);
    SearchLimits limits;
    limits.depth = depth;
    return searcher.search(Position(fen), limits);
}

}  // namespace

TEST(Search, FindsMateInOne) {
    // Back rank mate: Re8#.
    const SearchResult r = search_depth("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1", 4);
    EXPECT_EQ(r.best_move.to_uci(), "e1e8");
    EXPECT_EQ(uci_score(r.score), "mate 1");
}

TEST(Search, FindsMateInTwo) {
    // 1.Qd8+ Bxd8 2.Re8#.
    const SearchResult r = search_depth("r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 1 0", 6);
    EXPECT_EQ(uci_score(r.score), "mate 2");
    EXPECT_EQ(r.best_move.to_uci(), "d5d8");
}

TEST(Search, SeesBeingMated) {
    // Black's only move is Kg8, after which Ra8 is mate.
    const SearchResult r = search_depth("7k/R7/6K1/8/8/8/8/8 b - - 0 1", 4);
    EXPECT_TRUE(r.best_move);
    EXPECT_EQ(uci_score(r.score), "mate -1");
}

TEST(Search, WinsHangingQueen) {
    const SearchResult r = search_depth("rnb1kbnr/pppp1ppp/8/4p1q1/4P3/3P4/PPP2PPP/RNBQKBNR w KQkq - 0 1", 5);
    EXPECT_EQ(r.best_move.to_uci(), "c1g5");
}

TEST(Search, StalemateIsDraw) {
    TranspositionTable tt(1);
    Searcher searcher(tt);
    SearchLimits limits;
    limits.depth = 3;
    const SearchResult r = searcher.search(Position("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"), limits);
    EXPECT_FALSE(r.best_move);  // no legal move to return
}

TEST(Search, AvoidsStalematingWhenWinning) {
    // Qf7 would stalemate; the engine must find a mating line instead.
    const SearchResult r = search_depth("7k/8/6K1/8/8/8/8/5Q2 w - - 0 1", 6);
    EXPECT_NE(r.best_move.to_uci(), "f1f7");
    EXPECT_TRUE(uci_score(r.score).starts_with("mate "));
}

TEST(Search, ReturnsLegalMoveWithPv) {
    TranspositionTable tt(8);
    Searcher searcher(tt);
    SearchLimits limits;
    limits.depth = 6;
    std::vector<Move> last_pv;
    const Position root("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    const SearchResult r = searcher.search(root, limits, [&](const SearchInfo& info) { last_pv = info.pv; });
    EXPECT_TRUE(legal_moves(root).contains(r.best_move));
    ASSERT_FALSE(last_pv.empty());
    EXPECT_EQ(last_pv.front(), r.best_move);

    // Every move of the principal variation must be legal in sequence.
    Position pos = root;
    for (Move m : last_pv) {
        ASSERT_TRUE(legal_moves(pos).contains(m)) << m.to_uci();
        pos.make_move(m);
    }
}

TEST(Search, DeterministicAtFixedDepth) {
    const char* fen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
    const SearchResult a = search_depth(fen, 7);
    const SearchResult b = search_depth(fen, 7);
    EXPECT_EQ(a.best_move, b.best_move);
    EXPECT_EQ(a.nodes, b.nodes);
    EXPECT_EQ(a.score, b.score);
}

TEST(Search, RespectsMoveTime) {
    TranspositionTable tt(8);
    Searcher searcher(tt);
    SearchLimits limits;
    limits.movetime = 200;
    limits.move_overhead = 0;
    const auto start = std::chrono::steady_clock::now();
    const SearchResult r = searcher.search(Position(), limits);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_TRUE(r.best_move);
    EXPECT_LT(ms, 400);
}

TEST(Search, RespectsNodeLimit) {
    TranspositionTable tt(8);
    Searcher searcher(tt);
    SearchLimits limits;
    limits.nodes = 50'000;
    const SearchResult r = searcher.search(Position(), limits);
    EXPECT_TRUE(r.best_move);
    EXPECT_LT(r.nodes, 60'000u);
}

TEST(Search, ClaimsDrawByRepetitionWhenLosing) {
    // Black is a rook down but can force perpetual check with the queen.
    const SearchResult r = search_depth("6k1/5ppp/8/8/8/8/q4PPP/1R2R1K1 b - - 0 1", 8);
    EXPECT_TRUE(r.best_move);
    EXPECT_GT(r.score, -300);
}
