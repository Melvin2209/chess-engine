#include "tt.h"

#include <algorithm>

namespace chess {

void TranspositionTable::resize(std::size_t megabytes) {
    const std::size_t entries = std::max<std::size_t>(1, megabytes * 1024 * 1024 / sizeof(TTEntry));
    table_.assign(entries, TTEntry{});
    generation_ = 0;
}

void TranspositionTable::clear() {
    std::fill(table_.begin(), table_.end(), TTEntry{});
    generation_ = 0;
}

const TTEntry* TranspositionTable::probe(Key key) const {
    const TTEntry& e = table_[index(key)];
    return (e.key == key && e.bound() != BOUND_NONE) ? &e : nullptr;
}

void TranspositionTable::store(Key key, Move move, Score score, Score static_eval, int depth, Bound bound) {
    TTEntry& e = table_[index(key)];
    const bool same = e.key == key;
    // Replace when the slot holds another position from an older search, a shallower result,
    // or when we have an exact score. Keep the old best move if the new one is empty.
    if (!same && e.generation() == generation_ && e.depth > depth + 3 && bound != BOUND_EXACT) return;
    if (same && e.depth > depth + 2 && bound != BOUND_EXACT) return;

    if (move || !same) e.move = move.raw();
    e.key = key;
    e.score = std::int16_t(score);
    e.static_eval = std::int16_t(static_eval);
    e.depth = std::uint8_t(std::clamp(depth, 0, 255));
    e.bound_gen = std::uint8_t(bound | (generation_ << 2));
}

int TranspositionTable::hashfull() const {
    const std::size_t sample = std::min<std::size_t>(1000, table_.size());
    int used = 0;
    for (std::size_t i = 0; i < sample; ++i)
        if (table_[i].bound() != BOUND_NONE && table_[i].generation() == generation_) ++used;
    return int(used * 1000 / sample);
}

}  // namespace chess
