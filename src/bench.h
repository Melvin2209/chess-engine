#pragma once

#include <cstdint>
#include <iosfwd>

namespace chess {

constexpr int DEFAULT_BENCH_DEPTH = 13;

struct BenchResult {
    std::uint64_t nodes = 0;
    std::int64_t time_ms = 0;
    std::uint64_t nps() const { return time_ms > 0 ? nodes * 1000 / std::uint64_t(time_ms) : 0; }
};

// Searches a fixed set of positions to a fixed depth with a fresh hash table.
// The node count is deterministic (a "signature" for functional changes); NPS measures speed.
BenchResult run_bench(std::ostream& out, int depth = DEFAULT_BENCH_DEPTH);

}  // namespace chess
