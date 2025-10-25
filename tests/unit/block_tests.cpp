#include "ubuntu/core/block.h"
#include <gtest/gtest.h>

TEST(BlockTest, GenesisBlock) {
    auto genesis = ubuntu::core::createGenesisBlock();
    EXPECT_EQ(genesis.header.version, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
