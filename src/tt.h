#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "types.h"

namespace chess {

enum Bound : std::uint8_t { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

struct TTEntry {
    Key key = 0;
    std::uint16_t move = 0;
    std::int16_t score = 0;
    std::int16_t static_eval = 0;
    std::uint8_t depth = 0;
    std::uint8_t bound_gen = 0;  // low 2 bits: bound, high 6 bits: search generation

    Bound bound() const { return Bound(bound_gen & 3); }
    std::uint8_t generation() const { return bound_gen >> 2; }
};
static_assert(sizeof(TTEntry) == 16);

// Transposition table indexed by Zobrist key. Stored mate scores are relative to the node
// (see search.cpp) so they remain valid when the position is reached at a different ply.
class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes = 16) { resize(megabytes); }

    void resize(std::size_t megabytes);
    void clear();
    void new_search() { generation_ = (generation_ + 1) & 63; }

    // Returns the entry for `key` if present, otherwise nullptr.
    const TTEntry* probe(Key key) const;
    void store(Key key, Move move, Score score, Score static_eval, int depth, Bound bound);
    void prefetch(Key key) const {
#if defined(__GNUC__)
        __builtin_prefetch(&table_[index(key)]);
#else
        (void)key;
#endif
    }

    // Approximate fill rate in permille, as reported by UCI "hashfull".
    int hashfull() const;
    std::size_t size() const { return table_.size(); }

private:
    // Maps a 64-bit key uniformly onto [0, size) without a division ("fast range" reduction).
    std::size_t index(Key key) const {
#if defined(__SIZEOF_INT128__)
        __extension__ using u128 = unsigned __int128;
        return std::size_t((u128(key) * table_.size()) >> 64);
#else
        return std::size_t(key % table_.size());
#endif
    }

    std::vector<TTEntry> table_;
    std::uint8_t generation_ = 0;
};

}  // namespace chess
