#include "search.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "eval.h"

namespace chess {

namespace {

// Late move reduction table, indexed by [depth][move number].
struct LmrTable {
    int r[64][64];
    LmrTable() {
        for (int d = 0; d < 64; ++d)
            for (int m = 0; m < 64; ++m)
                r[d][m] = (d && m) ? int(0.75 + std::log(d) * std::log(m) / 2.25) : 0;
    }
};
const LmrTable lmr;

constexpr int HISTORY_MAX = 16384;

// Mate scores are stored relative to the current node rather than the root.
Score score_to_tt(Score s, int ply) {
    if (s >= SCORE_MATE_IN_MAX) return s + ply;
    if (s <= -SCORE_MATE_IN_MAX) return s - ply;
    return s;
}

Score score_from_tt(Score s, int ply) {
    if (s >= SCORE_MATE_IN_MAX) return s - ply;
    if (s <= -SCORE_MATE_IN_MAX) return s + ply;
    return s;
}

PieceType captured_type(const Position& pos, Move m) {
    if (m.is_en_passant()) return PAWN;
    const Piece p = pos.piece_on(m.to());
    return p == NO_PIECE ? NO_PIECE_TYPE : type_of(p);
}

// Moves the best-scored remaining move to index i (lazy selection sort).
void pick_move(MoveList& list, int* scores, int i) {
    int best = i;
    for (int j = i + 1; j < list.size(); ++j)
        if (scores[j] > scores[best]) best = j;
    std::swap(list[i], list[best]);
    std::swap(scores[i], scores[best]);
}

void update_history(int& entry, int bonus) {
    entry += bonus - entry * std::abs(bonus) / HISTORY_MAX;
}

}  // namespace

std::string uci_score(Score s) {
    if (std::abs(s) >= SCORE_MATE_IN_MAX) {
        const int plies = SCORE_MATE - std::abs(s);
        const int moves = (plies + 1) / 2;
        return "mate " + std::to_string(s > 0 ? moves : -moves);
    }
    return "cp " + std::to_string(s);
}

Searcher::Searcher(TranspositionTable& tt) : tt_(tt) { clear_history(); }

void Searcher::clear_history() {
    std::memset(history_, 0, sizeof history_);
    for (auto& k : killers_) k[0] = k[1] = Move::none();
}

std::int64_t Searcher::elapsed_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count();
}

void Searcher::setup_time(const Position& root, const SearchLimits& limits) {
    soft_limit_ms_ = hard_limit_ms_ = 0;
    if (limits.infinite) return;

    const Color us = root.side_to_move();
    if (limits.movetime > 0) {
        soft_limit_ms_ = hard_limit_ms_ = std::max<std::int64_t>(1, limits.movetime - limits.move_overhead);
        return;
    }
    if (!limits.has_clock(us)) return;

    const std::int64_t available = std::max<std::int64_t>(1, limits.time[us] - limits.move_overhead);
    const int moves_to_go = limits.movestogo > 0 ? std::min(limits.movestogo, 50) : 30;
    const std::int64_t optimum = available / moves_to_go + limits.inc[us] * 3 / 4;
    const std::int64_t maximum = std::min<std::int64_t>(available * 3 / 4, optimum * 4);
    hard_limit_ms_ = std::max<std::int64_t>(1, maximum);
    // Starting a new iteration is pointless if it is unlikely to finish in time.
    soft_limit_ms_ = std::max<std::int64_t>(1, std::min(optimum, maximum) * 6 / 10);
}

void Searcher::check_limits() {
    // Never abort before the first iteration has produced a move.
    if (root_depth_ <= 1) return;
    if (hard_limit_ms_ && elapsed_ms() >= hard_limit_ms_) stop();
    if (limits_.nodes && nodes_ >= limits_.nodes) stop();
}

SearchResult Searcher::search(const Position& root, const SearchLimits& limits, const InfoCallback& on_info) {
    start_ = Clock::now();
    limits_ = limits;
    nodes_ = 0;
    stop_.store(false, std::memory_order_relaxed);
    setup_time(root, limits);
    tt_.new_search();

    // Keep history between moves of a game, but let old information fade.
    for (auto& side : history_)
        for (auto& from : side)
            for (int& h : from) h /= 2;
    for (auto& k : killers_) k[0] = k[1] = Move::none();

    Position pos = root;
    SearchResult result;
    const MoveList root_moves = legal_moves(pos);
    if (root_moves.empty()) return result;
    result.best_move = root_moves[0];

    const bool time_managed = soft_limit_ms_ > 0 && limits.movetime == 0;
    Score previous = 0;

    for (int depth = 1; depth <= std::min(limits.depth, MAX_PLY - 1); ++depth) {
        root_depth_ = depth;
        seldepth_ = 0;

        // Aspiration window around the previous score; widened on failure.
        Score delta = 25;
        Score alpha = -SCORE_INF, beta = SCORE_INF;
        if (depth >= 5) {
            alpha = std::max<Score>(previous - delta, -SCORE_INF);
            beta = std::min<Score>(previous + delta, SCORE_INF);
        }

        Score score = 0;
        for (;;) {
            score = negamax(pos, depth, alpha, beta, 0, false);
            if (stop_.load(std::memory_order_relaxed)) break;
            if (score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max<Score>(score - delta, -SCORE_INF);
            } else if (score >= beta) {
                beta = std::min<Score>(score + delta, SCORE_INF);
            } else {
                break;
            }
            delta += delta / 2;
        }

        if (stop_.load(std::memory_order_relaxed) && depth > 1) break;

        previous = score;
        result.best_move = pv_length_[0] > 0 ? pv_[0][0] : result.best_move;
        result.ponder_move = pv_length_[0] > 1 ? pv_[0][1] : Move::none();
        result.score = score;
        result.depth = depth;

        if (on_info) {
            SearchInfo info;
            info.depth = depth;
            info.seldepth = seldepth_;
            info.score = score;
            info.nodes = nodes_;
            info.time_ms = elapsed_ms();
            info.hashfull = tt_.hashfull();
            info.pv.assign(pv_[0], pv_[0] + pv_length_[0]);
            on_info(info);
        }

        if (time_managed) {
            if (elapsed_ms() >= soft_limit_ms_) break;
            if (root_moves.size() == 1) break;
            if (std::abs(score) >= SCORE_MATE_IN_MAX && SCORE_MATE - std::abs(score) <= depth) break;
        }
        if (limits.nodes && nodes_ >= limits.nodes) break;
        if (stop_.load(std::memory_order_relaxed)) break;
    }

    result.nodes = nodes_;
    return result;
}

void Searcher::score_moves(const Position& pos, MoveList& list, int* scores, Move tt_move, int ply) const {
    const Color us = pos.side_to_move();
    for (int i = 0; i < list.size(); ++i) {
        const Move m = list[i];
        int s;
        if (m == tt_move) {
            s = 10'000'000;
        } else if (m.is_capture() || m.is_promotion()) {
            // MVV-LVA: most valuable victim first, then least valuable attacker.
            const int victim = eval::SIMPLE_VALUE[captured_type(pos, m)];
            const int attacker = eval::SIMPLE_VALUE[type_of(pos.piece_on(m.from()))];
            s = victim * 10 - attacker / 10;
            if (m.is_promotion()) s += eval::SIMPLE_VALUE[m.promotion_type()];
            // Losing captures (by static exchange) are tried after the quiet moves.
            s += pos.see_ge(m, -50) ? 2'000'000 : -2'000'000;
        } else if (m == killers_[ply][0]) {
            s = 1'000'000;
        } else if (m == killers_[ply][1]) {
            s = 900'000;
        } else {
            s = history_[us][m.from()][m.to()];
        }
        scores[i] = s;
    }
}

void Searcher::update_quiet_stats(const Position& pos, Move best, const Move* quiets, int quiet_count, int depth,
                                  int ply) {
    const Color us = pos.side_to_move();
    const int bonus = std::min(depth * depth * 16, 1600);
    update_history(history_[us][best.from()][best.to()], bonus);
    for (int i = 0; i < quiet_count; ++i) update_history(history_[us][quiets[i].from()][quiets[i].to()], -bonus);
    if (killers_[ply][0] != best) {
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = best;
    }
}

Score Searcher::negamax(Position& pos, int depth, Score alpha, Score beta, int ply, bool cut_node) {
    pv_length_[ply] = ply;
    if (depth <= 0) return qsearch(pos, alpha, beta, ply);

    const bool pv_node = beta - alpha > 1;
    const bool root = ply == 0;

    if ((++nodes_ & 1023) == 0) check_limits();
    if (stop_.load(std::memory_order_relaxed)) return 0;
    seldepth_ = std::max(seldepth_, ply);

    if (!root) {
        if (pos.is_repetition() || pos.is_fifty_move_draw() || pos.has_insufficient_material()) return 0;
        if (ply >= MAX_PLY) return evaluate(pos);

        // Mate distance pruning: no line from here can beat a mate already found closer to the root.
        alpha = std::max<Score>(alpha, -SCORE_MATE + ply);
        beta = std::min<Score>(beta, SCORE_MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    const bool in_check = pos.in_check();
    const Key key = pos.key();
    const TTEntry* tte = tt_.probe(key);
    const Move tt_move = tte ? Move::from_raw(tte->move) : Move::none();

    if (tte && !pv_node && tte->depth >= depth) {
        const Score s = score_from_tt(tte->score, ply);
        const Bound b = tte->bound();
        if (b == BOUND_EXACT || (b == BOUND_LOWER && s >= beta) || (b == BOUND_UPPER && s <= alpha)) return s;
    }

    Score static_eval = SCORE_NONE;
    if (!in_check) static_eval = (tte && tte->static_eval != SCORE_NONE) ? tte->static_eval : evaluate(pos);
    static_evals_[ply] = static_eval;
    const bool improving =
        !in_check && ply >= 2 && static_evals_[ply - 2] != SCORE_NONE && static_eval > static_evals_[ply - 2];

    if (!pv_node && !in_check) {
        // Reverse futility pruning: far above beta at shallow depth, assume a cutoff.
        if (depth <= 6 && std::abs(beta) < SCORE_MATE_IN_MAX && static_eval - 80 * (depth - improving) >= beta)
            return static_eval;

        // Null move pruning: if passing still fails high, the position is very likely good enough.
        if (depth >= 3 && static_eval >= beta && pos.plies_from_null() > 0 &&
            pos.has_non_pawn_material(pos.side_to_move())) {
            const int r = 3 + depth / 4 + std::min(3, (static_eval - beta) / 200);
            pos.make_null_move();
            tt_.prefetch(pos.key());
            const Score s = -negamax(pos, depth - 1 - r, -beta, -beta + 1, ply + 1, !cut_node);
            pos.unmake_null_move();
            if (stop_.load(std::memory_order_relaxed)) return 0;
            if (s >= beta) return s >= SCORE_MATE_IN_MAX ? beta : s;
        }
    }

    // Internal iterative reduction: without a hash move, ordering is poor, so search shallower.
    if (depth >= 4 && !tt_move && (pv_node || cut_node)) --depth;

    MoveList list;
    generate<GenType::ALL>(pos, list);
    if (list.empty()) return in_check ? -SCORE_MATE + ply : 0;

    int scores[MoveList::CAPACITY];
    score_moves(pos, list, scores, tt_move, ply);

    Score best = -SCORE_INF;
    Move best_move = Move::none();
    Move quiets[64];
    int quiet_count = 0;
    int moves_searched = 0;

    for (int i = 0; i < list.size(); ++i) {
        pick_move(list, scores, i);
        const Move m = list[i];
        const bool quiet = m.is_quiet();

        // Shallow-depth pruning once a non-losing line is known.
        if (!root && !in_check && best > -SCORE_MATE_IN_MAX) {
            if (quiet) {
                // Late move pruning.
                if (!pv_node && depth <= 4 && moves_searched >= 3 + depth * depth * (improving ? 2 : 1)) continue;
                // Futility pruning.
                if (!pv_node && depth <= 4 && static_eval + 100 + 90 * depth <= alpha) continue;
            } else if (depth <= 4 && !pos.see_ge(m, -100 * depth)) {
                continue;
            }
        }

        pos.make_move(m);
        tt_.prefetch(pos.key());
        ++moves_searched;

        const bool gives_check = pos.in_check();
        // Check extension, bounded so that perpetual checks cannot blow up the tree.
        const int new_depth = depth - 1 + (gives_check && ply < 2 * root_depth_ ? 1 : 0);

        Score score;
        if (moves_searched == 1) {
            score = -negamax(pos, new_depth, -beta, -alpha, ply + 1, false);
        } else {
            int r = 0;
            if (depth >= 3 && quiet && moves_searched > 2) {
                r = lmr.r[std::min(depth, 63)][std::min(moves_searched, 63)];
                r -= pv_node;
                r += cut_node;
                r += !improving;
                r -= gives_check;
                r -= (m == killers_[ply][0] || m == killers_[ply][1]);
                r -= history_[~pos.side_to_move()][m.from()][m.to()] / 6000;
                r = std::clamp(r, 0, std::max(new_depth - 1, 0));
            }
            // Principal variation search: prove the move is no better with a null window first.
            score = -negamax(pos, new_depth - r, -alpha - 1, -alpha, ply + 1, true);
            if (score > alpha && r > 0) score = -negamax(pos, new_depth, -alpha - 1, -alpha, ply + 1, !cut_node);
            if (pv_node && score > alpha && score < beta) score = -negamax(pos, new_depth, -beta, -alpha, ply + 1, false);
        }
        pos.unmake_move(m);
        if (stop_.load(std::memory_order_relaxed)) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) {
                best_move = m;
                alpha = score;
                pv_[ply][ply] = m;
                for (int j = ply + 1; j < pv_length_[ply + 1]; ++j) pv_[ply][j] = pv_[ply + 1][j];
                pv_length_[ply] = std::max(pv_length_[ply + 1], ply + 1);
                if (alpha >= beta) {
                    if (quiet) update_quiet_stats(pos, m, quiets, quiet_count, depth, ply);
                    break;
                }
            }
        }
        if (quiet && quiet_count < 64) quiets[quiet_count++] = m;
    }

    const Bound bound = best >= beta ? BOUND_LOWER : (best_move ? BOUND_EXACT : BOUND_UPPER);
    tt_.store(key, best_move, score_to_tt(best, ply), static_eval, depth, bound);
    return best;
}

Score Searcher::qsearch(Position& pos, Score alpha, Score beta, int ply) {
    pv_length_[ply] = ply;
    if ((++nodes_ & 1023) == 0) check_limits();
    if (stop_.load(std::memory_order_relaxed)) return 0;
    seldepth_ = std::max(seldepth_, ply);

    if (pos.is_fifty_move_draw() || pos.has_insufficient_material()) return 0;
    if (ply >= MAX_PLY) return evaluate(pos);

    const bool pv_node = beta - alpha > 1;
    const bool in_check = pos.in_check();
    const Key key = pos.key();
    const TTEntry* tte = tt_.probe(key);
    if (tte && !pv_node) {
        const Score s = score_from_tt(tte->score, ply);
        const Bound b = tte->bound();
        if (b == BOUND_EXACT || (b == BOUND_LOWER && s >= beta) || (b == BOUND_UPPER && s <= alpha)) return s;
    }

    Score best;
    Score stand_pat = SCORE_NONE;
    MoveList list;
    if (in_check) {
        // No stand pat when in check: every evasion must be considered.
        best = -SCORE_MATE + ply;
        generate<GenType::ALL>(pos, list);
    } else {
        stand_pat = (tte && tte->static_eval != SCORE_NONE) ? tte->static_eval : evaluate(pos);
        if (stand_pat >= beta) return stand_pat;
        alpha = std::max(alpha, stand_pat);
        best = stand_pat;
        generate<GenType::NOISY>(pos, list);
    }

    int scores[MoveList::CAPACITY];
    score_moves(pos, list, scores, tte ? Move::from_raw(tte->move) : Move::none(), ply);
    const Score original_alpha = alpha;
    Move best_move = Move::none();

    for (int i = 0; i < list.size(); ++i) {
        pick_move(list, scores, i);
        const Move m = list[i];
        if (!in_check && !m.is_promotion()) {
            // Delta pruning: even winning the captured piece outright cannot raise alpha.
            if (stand_pat + eval::SIMPLE_VALUE[captured_type(pos, m)] + 200 <= alpha) continue;
            if (!pos.see_ge(m, 0)) continue;
        }

        pos.make_move(m);
        const Score score = -qsearch(pos, -beta, -alpha, ply + 1);
        pos.unmake_move(m);
        if (stop_.load(std::memory_order_relaxed)) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) {
                alpha = score;
                best_move = m;
                pv_[ply][ply] = m;
                for (int j = ply + 1; j < pv_length_[ply + 1]; ++j) pv_[ply][j] = pv_[ply + 1][j];
                pv_length_[ply] = std::max(pv_length_[ply + 1], ply + 1);
                if (alpha >= beta) break;
            }
        }
    }

    const Bound bound = best >= beta ? BOUND_LOWER : (alpha > original_alpha ? BOUND_EXACT : BOUND_UPPER);
    tt_.store(key, best_move, score_to_tt(best, ply), stand_pat, 0, bound);
    return best;
}

}  // namespace chess
