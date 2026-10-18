#include <gtest/gtest.h>

#include <algorithm>
#include <random>

#include "movegen.h"
#include "position.h"

using namespace chess;

namespace {
bool has_move(const Position& pos, const char* uci) { return bool(pos.parse_uci_move(uci)); }
}  // namespace

TEST(MoveGen, StartposHasTwentyMoves) { EXPECT_EQ(legal_moves(Position()).size(), 20); }

TEST(MoveGen, CheckmateHasNoMoves) {
    // Fool's mate.
    Position pos("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    EXPECT_TRUE(pos.in_check());
    EXPECT_TRUE(legal_moves(pos).empty());
}

TEST(MoveGen, StalemateHasNoMoves) {
    Position pos("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    EXPECT_FALSE(pos.in_check());
    EXPECT_TRUE(legal_moves(pos).empty());
}

TEST(MoveGen, CannotCastleThroughAttackedSquare) {
    // The black rook on f8 covers f1.
    Position pos("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT_FALSE(has_move(pos, "e1g1"));
    EXPECT_TRUE(has_move(pos, "e1c1"));
}

TEST(MoveGen, CannotCastleOutOfCheck) {
    Position pos("4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1");
    EXPECT_FALSE(has_move(pos, "e1g1"));
    EXPECT_FALSE(has_move(pos, "e1c1"));
}

TEST(MoveGen, LongCastleAllowedWhenOnlyB1Attacked) {
    Position pos("1r2k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
    EXPECT_TRUE(has_move(pos, "e1c1"));
}

TEST(MoveGen, EnPassantBlockedByHorizontalPin) {
    // Capturing en passant would expose the white king on a5 to the rook on h5.
    Position pos("8/8/8/KPp4r/8/8/8/7k w - c6 0 1");
    EXPECT_FALSE(has_move(pos, "b5c6"));
}

TEST(MoveGen, EnPassantResolvesPawnCheck) {
    Position pos("8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1");
    ASSERT_TRUE(pos.in_check());
    EXPECT_TRUE(has_move(pos, "e4d3"));
}

TEST(MoveGen, PinnedPieceMovesAlongPinOnly) {
    Position pos("4k3/4r3/8/8/8/8/4R3/4K3 w - - 0 1");
    for (Move m : legal_moves(pos)) {
        if (m.from() == E2) {
            EXPECT_EQ(file_of(m.to()), file_of(E2)) << m.to_uci();
        }
    }
    EXPECT_TRUE(has_move(pos, "e2e7"));
}

TEST(MoveGen, AllPromotionPiecesGenerated) {
    Position pos("8/P6k/8/8/8/8/8/K7 w - - 0 1");
    for (const char* m : {"a7a8q", "a7a8r", "a7a8b", "a7a8n"}) EXPECT_TRUE(has_move(pos, m)) << m;
}

// The noisy generator must return exactly the captures and queen promotions of the full list.
TEST(MoveGen, NoisyIsSubsetOfAll) {
    std::mt19937 rng(42);
    for (int game = 0; game < 100; ++game) {
        Position pos;
        for (int ply = 0; ply < 100; ++ply) {
            const MoveList all = legal_moves(pos);
            if (all.empty()) break;
            MoveList noisy;
            generate<GenType::NOISY>(pos, noisy);

            int expected = 0;
            for (Move m : all) {
                const bool is_noisy =
                    m.is_capture() || (m.is_promotion() && m.promotion_type() == QUEEN);
                if (is_noisy) {
                    ++expected;
                    EXPECT_TRUE(noisy.contains(m)) << pos.fen() << ' ' << m.to_uci();
                }
            }
            EXPECT_EQ(noisy.size(), expected) << pos.fen();
            pos.make_move(all[int(rng() % unsigned(all.size()))]);
        }
    }
}

TEST(MoveGen, UciMoveStrings) {
    EXPECT_EQ(Move(E2, E4, DOUBLE_PUSH).to_uci(), "e2e4");
    EXPECT_EQ(Move(A7, A8, PROMO_QUEEN).to_uci(), "a7a8q");
    EXPECT_EQ(Move(B7, C8, PROMO_CAPTURE_KNIGHT).to_uci(), "b7c8n");
    EXPECT_EQ(Move::none().to_uci(), "0000");
}
