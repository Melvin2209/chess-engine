#include "bench.h"

#include <chrono>
#include <iomanip>
#include <ostream>

#include "search.h"

namespace chess {

namespace {

constexpr const char* BENCH_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r2q1rk1/pp2bppp/2n1bn2/2pp4/3P4/2PBPN2/PP1N1PPP/R2QK2R w KQ - 0 10",
    "2r3k1/5pp1/p3p2p/1p1n4/3P4/P2B1N2/1P3PPP/2R3K1 w - - 0 25",
    "8/8/4k3/8/2p5/8/B2K4/8 w - - 0 1",
    "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1",
    "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4R1K w - - 0 15",
};

}  // namespace

BenchResult run_bench(std::ostream& out, int depth) {
    TranspositionTable tt(16);
    Searcher searcher(tt);
    BenchResult total;
    const auto start = std::chrono::steady_clock::now();

    int index = 1;
    for (const char* fen : BENCH_FENS) {
        tt.clear();
        searcher.clear_history();
        SearchLimits limits;
        limits.depth = depth;
        const SearchResult r = searcher.search(Position(fen), limits);
        total.nodes += r.nodes;
        out << "Position " << std::setw(2) << index++ << ": bestmove " << std::setw(5) << r.best_move.to_uci()
            << "  score " << std::setw(6) << r.score << "  nodes " << r.nodes << '\n';
    }

    total.time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    out << "===========================\n"
        << "Total time (ms) : " << total.time_ms << '\n'
        << "Nodes searched  : " << total.nodes << '\n'
        << "Nodes/second    : " << total.nps() << std::endl;
    return total;
}

}  // namespace chess
