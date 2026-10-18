#include <iostream>
#include <string>

#include "bench.h"
#include "bitboard.h"
#include "position.h"
#include "uci.h"

int main(int argc, char* argv[]) {
    chess::attacks::init();
    chess::zobrist::init();

    // "chess-engine bench [depth]" runs the benchmark and exits (used by CI and OpenBench-style tooling).
    if (argc > 1 && std::string(argv[1]) == "bench") {
        const int depth = argc > 2 ? std::stoi(argv[2]) : chess::DEFAULT_BENCH_DEPTH;
        chess::run_bench(std::cout, depth);
        return 0;
    }

    chess::UciEngine engine(std::cin, std::cout);
    engine.loop();
    return 0;
}
