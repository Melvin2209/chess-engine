// Perft results for the standard reference positions from the Chess Programming Wiki
// (https://www.chessprogramming.org/Perft_Results) plus a set of edge cases that
// exercise discovered checks, en passant pins, castling rights and promotions.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "movegen.h"
#include "position.h"

namespace {

struct PerftCase {
    std::string name;
    std::string fen;
    int depth;
    std::uint64_t nodes;
};

void PrintTo(const PerftCase& c, std::ostream* os) { *os << c.name << " depth " << c.depth; }

class Perft : public ::testing::TestWithParam<PerftCase> {};

TEST_P(Perft, MatchesReferenceNodeCount) {
    const PerftCase& c = GetParam();
    chess::Position pos(c.fen);
    const std::string fen_before = pos.fen();
    EXPECT_EQ(chess::perft(pos, c.depth), c.nodes);
    EXPECT_EQ(pos.fen(), fen_before) << "make/unmake did not restore the position";
}

// Fast enough to run on every build (a few seconds in Release).
INSTANTIATE_TEST_SUITE_P(
    Reference, Perft,
    ::testing::Values(
        PerftCase{"Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4'865'609},
        PerftCase{"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4'085'603},
        PerftCase{"Position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11'030'083},
        PerftCase{"Position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15'833'292},
        PerftCase{"Position4Mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422'333},
        PerftCase{"Position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2'103'487},
        PerftCase{"Position6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
                  3'894'594}),
    [](const auto& info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(
    EdgeCases, Perft,
    ::testing::Values(
        PerftCase{"EpDiscoveredCheck", "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1'134'888},
        PerftCase{"BishopVsPawn", "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1'015'133},
        PerftCase{"EpPinnedByBishop", "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1'440'467},
        PerftCase{"ShortCastleOnly", "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661'072},
        PerftCase{"LongCastleOnly", "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803'711},
        PerftCase{"CastlingWithBishops", "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1'274'206},
        PerftCase{"CastlingUnderAttack", "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1'720'476},
        PerftCase{"PromotionOutOfCheck", "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3'821'001},
        PerftCase{"DiscoveredCheck", "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1'004'658},
        PerftCase{"PromotionCheck", "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217'342},
        PerftCase{"UnderPromotionCheck", "8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92'683},
        PerftCase{"SelfStalemate", "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2'217},
        PerftCase{"StalemateCheckmate", "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567'584},
        PerftCase{"DoubleCheck", "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23'527}),
    [](const auto& info) { return info.param.name; });

// Deep runs (hundreds of millions of nodes). Run with --gtest_also_run_disabled_tests.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_Deep, Perft,
    ::testing::Values(
        PerftCase{"Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119'060'324},
        PerftCase{"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5,
                  193'690'690},
        PerftCase{"Position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178'633'661},
        PerftCase{"Position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6,
                  706'045'033},
        PerftCase{"Position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89'941'194},
        PerftCase{"Position6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5,
                  164'075'551}),
    [](const auto& info) { return info.param.name; });

}  // namespace
