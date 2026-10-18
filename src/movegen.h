#pragma once

#include <cstdint>

#include "position.h"

namespace chess {

class MoveList {
public:
    static constexpr int CAPACITY = 256;  // The known maximum number of legal moves is 218.

    void push_back(Move m) { moves_[size_++] = m; }
    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
    Move operator[](int i) const { return moves_[i]; }
    Move& operator[](int i) { return moves_[i]; }
    Move* begin() { return moves_; }
    Move* end() { return moves_ + size_; }
    const Move* begin() const { return moves_; }
    const Move* end() const { return moves_ + size_; }
    bool contains(Move m) const;

private:
    Move moves_[CAPACITY];
    int size_ = 0;
};

enum class GenType {
    ALL,    // every legal move
    NOISY,  // legal captures (including en passant and capture-promotions) plus queen promotions
};

// Generates strictly legal moves: no make/unmake is needed to filter them afterwards.
template <GenType Type>
void generate(const Position& pos, MoveList& list);

inline MoveList legal_moves(const Position& pos) {
    MoveList list;
    generate<GenType::ALL>(pos, list);
    return list;
}

// Counts leaf nodes of the legal move tree to the given depth (standard move generator test).
std::uint64_t perft(Position& pos, int depth);

}  // namespace chess
