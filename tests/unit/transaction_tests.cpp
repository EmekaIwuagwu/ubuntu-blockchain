#include "ubuntu/core/transaction.h"
#include <gtest/gtest.h>

TEST(TransactionTest, BasicConstruction) {
    ubuntu::core::Transaction tx;
    EXPECT_EQ(tx.version, 1);
    EXPECT_TRUE(tx.inputs.empty());
    EXPECT_TRUE(tx.outputs.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
