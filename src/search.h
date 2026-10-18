#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "movegen.h"
#include "position.h"
#include "tt.h"

namespace chess {

struct SearchLimits {
    int depth = MAX_PLY - 1;
    std::int64_t movetime = 0;  // milliseconds; 0 = not set
    std::int64_t time[COLOR_NB] = {0, 0};
    std::int64_t inc[COLOR_NB] = {0, 0};
    int movestogo = 0;
    std::uint64_t nodes = 0;  // 0 = unlimited
    bool infinite = false;
    std::int64_t move_overhead = 30;  // safety margin for GUI/network lag, in ms

    bool has_clock(Color c) const { return time[c] > 0; }
};

struct SearchInfo {
    int depth = 0;
    int seldepth = 0;
    Score score = 0;
    std::uint64_t nodes = 0;
    std::int64_t time_ms = 0;
    int hashfull = 0;
    std::vector<Move> pv;
};

struct SearchResult {
    Move best_move;
    Move ponder_move;
    Score score = 0;
    int depth = 0;
    std::uint64_t nodes = 0;
};

using InfoCallback = std::function<void(const SearchInfo&)>;

// Formats a score the way UCI expects: "cp 34" or "mate -3".
std::string uci_score(Score s);

class Searcher {
public:
    explicit Searcher(TranspositionTable& tt);

    // Iterative deepening search; blocks until a limit is reached or stop() is called.
    SearchResult search(const Position& root, const SearchLimits& limits, const InfoCallback& on_info = {});

    // Thread-safe: may be called from the UCI thread while search() runs.
    void stop() { stop_.store(true, std::memory_order_relaxed); }

    // Forget move-ordering statistics (called on "ucinewgame").
    void clear_history();

private:
    using Clock = std::chrono::steady_clock;

    Score negamax(Position& pos, int depth, Score alpha, Score beta, int ply, bool cut_node);
    Score qsearch(Position& pos, Score alpha, Score beta, int ply);
    void score_moves(const Position& pos, MoveList& list, int* scores, Move tt_move, int ply) const;
    void update_quiet_stats(const Position& pos, Move best, const Move* quiets, int quiet_count, int depth, int ply);
    void check_limits();
    std::int64_t elapsed_ms() const;
    void setup_time(const Position& root, const SearchLimits& limits);

    TranspositionTable& tt_;
    std::atomic<bool> stop_{false};
    SearchLimits limits_;
    Clock::time_point start_;
    std::int64_t soft_limit_ms_ = 0;  // don't start a new iteration after this
    std::int64_t hard_limit_ms_ = 0;  // abort the search after this
    std::uint64_t nodes_ = 0;
    int seldepth_ = 0;
    int root_depth_ = 0;

    Move pv_[MAX_PLY + 1][MAX_PLY + 1];
    int pv_length_[MAX_PLY + 1];
    Move killers_[MAX_PLY + 1][2];
    int history_[COLOR_NB][SQUARE_NB][SQUARE_NB];
    Score static_evals_[MAX_PLY + 4];
};

}  // namespace chess
