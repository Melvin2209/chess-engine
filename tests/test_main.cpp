#include <gtest/gtest.h>

#include "bitboard.h"
#include "position.h"

int main(int argc, char** argv) {
    chess::attacks::init();
    chess::zobrist::init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
