#include <gtest/gtest.h>

#include <random>
#include <stdexcept>

#include "movegen.h"
#include "position.h"

using namespace chess;

TEST(Position, StartposFenRoundTrip) {
    Position pos;
    EXPECT_EQ(pos.fen(), Position::START_FEN);
    EXPECT_EQ(pos.side_to_move(), WHITE);
    EXPECT_EQ(pos.castling_rights(), ALL_CASTLING);
    EXPECT_EQ(pos.piece_on(E1), W_KING);
    EXPECT_EQ(pos.piece_on(D8), B_QUEEN);
}

TEST(Position, FenRoundTripForReferencePositions) {
    for (const char* fen : {
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
             "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
             "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
             "8/8/8/8/8/8/8/K6k b - - 99 120",
         }) {
        EXPECT_EQ(Position(fen).fen(), fen);
    }
}

TEST(Position, RejectsMalformedFen) {
    EXPECT_THROW(Position("not a fen"), std::invalid_argument);
    EXPECT_THROW(Position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1"), std::invalid_argument);
    EXPECT_THROW(Position("rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), std::invalid_argument);
    EXPECT_THROW(Position("8/8/8/8/8/8/8/8 w - - 0 1"), std::invalid_argument);
    EXPECT_THROW(Position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"), std::invalid_argument);
}

TEST(Position, MakeUnmakeRestoresEverything) {
    Position pos("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    const std::string fen = pos.fen();
    const Key key = pos.key();
    for (Move m : legal_moves(pos)) {
        pos.make_move(m);
        pos.unmake_move(m);
        ASSERT_EQ(pos.fen(), fen) << m.to_uci();
        ASSERT_EQ(pos.key(), key) << m.to_uci();
    }
}

// Plays random games and checks that the incrementally updated hashes always match
// a from-scratch computation, and that a FEN round trip yields the same key.
TEST(Position, IncrementalZobristMatchesFullRecomputation) {
    std::mt19937 rng(12345);
    for (int game = 0; game < 200; ++game) {
        Position pos;
        for (int ply = 0; ply < 120; ++ply) {
            const MoveList moves = legal_moves(pos);
            if (moves.empty()) break;
            const Move m = moves[int(rng() % unsigned(moves.size()))];
            pos.make_move(m);
            ASSERT_EQ(pos.key(), pos.compute_key()) << pos.fen();
            ASSERT_EQ(pos.pawn_key(), pos.compute_pawn_key()) << pos.fen();
            ASSERT_EQ(Position(pos.fen()).key(), pos.key()) << pos.fen();
        }
    }
}

TEST(Position, TranspositionsHashEqually) {
    Position a, b;
    for (const char* m : {"g1f3", "g8f6", "b1c3", "b8c6"}) a.make_move(a.parse_uci_move(m));
    for (const char* m : {"b1c3", "b8c6", "g1f3", "g8f6"}) b.make_move(b.parse_uci_move(m));
    EXPECT_EQ(a.key(), b.key());
}

TEST(Position, EnPassantOnlyHashedWhenCapturable) {
    // After 1.e4 no black pawn can capture on e3, so the position equals the one without the ep square.
    Position pos;
    pos.make_move(pos.parse_uci_move("e2e4"));
    EXPECT_EQ(pos.key(), Position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1").key());

    Position capturable("rnbqkbnr/ppp1pppp/8/8/3p4/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    capturable.make_move(capturable.parse_uci_move("e2e4"));
    EXPECT_NE(capturable.key(), Position("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1").key());
}

TEST(Position, CastlingRightsUpdate) {
    Position pos("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    pos.make_move(pos.parse_uci_move("h1h8"));  // rook takes rook: both kingside rights vanish
    EXPECT_EQ(pos.castling_rights(), WHITE_OOO | BLACK_OOO);
    pos.make_move(pos.parse_uci_move("e8d7"));
    EXPECT_EQ(pos.castling_rights(), WHITE_OOO);
}

TEST(Position, CastlingMovesRook) {
    Position pos("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    pos.make_move(pos.parse_uci_move("e1g1"));
    EXPECT_EQ(pos.piece_on(G1), W_KING);
    EXPECT_EQ(pos.piece_on(F1), W_ROOK);
    EXPECT_EQ(pos.piece_on(H1), NO_PIECE);
    pos.make_move(pos.parse_uci_move("e8c8"));
    EXPECT_EQ(pos.piece_on(C8), B_KING);
    EXPECT_EQ(pos.piece_on(D8), B_ROOK);
    EXPECT_EQ(pos.piece_on(A8), NO_PIECE);
}

TEST(Position, EnPassantCaptureRemovesPawn) {
    Position pos("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    const Move m = pos.parse_uci_move("e5f6");
    ASSERT_TRUE(m);
    EXPECT_TRUE(m.is_en_passant());
    pos.make_move(m);
    EXPECT_EQ(pos.piece_on(F5), NO_PIECE);
    EXPECT_EQ(pos.piece_on(F6), W_PAWN);
}

TEST(Position, PromotionAndUnmake) {
    Position pos("8/P6k/8/8/8/8/8/K7 w - - 0 1");
    const Move m = pos.parse_uci_move("a7a8n");
    ASSERT_TRUE(m);
    pos.make_move(m);
    EXPECT_EQ(pos.piece_on(A8), W_KNIGHT);
    pos.unmake_move(m);
    EXPECT_EQ(pos.piece_on(A7), W_PAWN);
    EXPECT_EQ(pos.piece_on(A8), NO_PIECE);
}

TEST(Position, DetectsRepetition) {
    Position pos;
    EXPECT_FALSE(pos.is_repetition());
    for (const char* m : {"g1f3", "g8f6", "f3g1", "f6g8"}) pos.make_move(pos.parse_uci_move(m));
    EXPECT_TRUE(pos.is_repetition());
}

TEST(Position, IrreversibleMoveResetsRepetitionWindow) {
    Position pos;
    for (const char* m : {"g1f3", "g8f6", "f3g1", "f6g8", "e2e4"}) pos.make_move(pos.parse_uci_move(m));
    EXPECT_FALSE(pos.is_repetition());
}

TEST(Position, FiftyMoveRule) {
    EXPECT_TRUE(Position("8/8/8/8/8/8/8/K5Rk w - - 100 80").is_fifty_move_draw());
    EXPECT_FALSE(Position("8/8/8/8/8/8/8/K5Rk w - - 99 80").is_fifty_move_draw());
}

TEST(Position, InsufficientMaterial) {
    EXPECT_TRUE(Position("8/8/8/8/8/8/8/K6k w - - 0 1").has_insufficient_material());
    EXPECT_TRUE(Position("8/8/8/8/8/8/8/KN5k w - - 0 1").has_insufficient_material());
    EXPECT_TRUE(Position("8/8/8/8/8/8/8/KB5k w - - 0 1").has_insufficient_material());
    EXPECT_FALSE(Position("8/8/8/8/8/8/8/KR5k w - - 0 1").has_insufficient_material());
    EXPECT_FALSE(Position("8/8/8/8/8/8/P7/K6k w - - 0 1").has_insufficient_material());
    EXPECT_FALSE(Position("8/8/8/8/8/8/8/KNN4k w - - 0 1").has_insufficient_material());
}

TEST(Position, CheckersAndPins) {
    Position pos("4k3/8/8/8/4r3/8/4B3/4K3 w - - 0 1");
    EXPECT_FALSE(pos.in_check());
    EXPECT_EQ(pos.pinned(WHITE), square_bb(E2));

    Position check("4k3/8/8/8/4r3/8/8/4K3 w - - 0 1");
    EXPECT_TRUE(check.in_check());
    EXPECT_EQ(check.checkers(), square_bb(E4));
}

TEST(Position, StaticExchangeEvaluation) {
    // Pawn takes undefended knight: clearly winning.
    Position a("4k3/8/8/3n4/4P3/8/8/4K3 w - - 0 1");
    EXPECT_TRUE(a.see_ge(a.parse_uci_move("e4d5"), 300));

    // Queen takes a pawn defended by a pawn: loses the queen for a pawn.
    Position b("4k3/8/2p5/3p4/8/8/3Q4/4K3 w - - 0 1");
    EXPECT_FALSE(b.see_ge(b.parse_uci_move("d2d5"), 0));

    // Rook takes a rook defended by a rook, backed by our second rook: equal trade.
    Position c("3rk3/3r4/8/8/8/8/3R4/3RK3 w - - 0 1");
    EXPECT_TRUE(c.see_ge(c.parse_uci_move("d2d7"), 0));
    EXPECT_FALSE(c.see_ge(c.parse_uci_move("d2d7"), 1));

    // X-ray: the second rook behind the first one joins the exchange and wins the pawn.
    Position d("3rk3/8/8/3p4/8/8/3R4/3RK3 w - - 0 1");
    EXPECT_TRUE(d.see_ge(d.parse_uci_move("d2d5"), 100));
    Position e("3rk3/8/8/3p4/8/8/3R4/4K3 w - - 0 1");
    EXPECT_FALSE(e.see_ge(e.parse_uci_move("d2d5"), 0));
}
