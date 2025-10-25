#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"

#include <gtest/gtest.h>

using namespace ubuntu::core;
using namespace ubuntu::crypto;

class TransactionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sample transaction
        tx.version = 1;
        tx.lockTime = 0;
    }

    Transaction tx;
};

// ===== Basic Transaction Tests =====

TEST_F(TransactionTest, DefaultConstruction) {
    Transaction empty;
    EXPECT_EQ(empty.version, 1);
    EXPECT_TRUE(empty.inputs.empty());
    EXPECT_TRUE(empty.outputs.empty());
    EXPECT_EQ(empty.lockTime, 0);
}

TEST_F(TransactionTest, AddInput) {
    TxInput input;
    input.previousOutput.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    input.previousOutput.vout = 0;
    input.sequence = 0xFFFFFFFF;

    tx.inputs.push_back(input);

    EXPECT_EQ(tx.inputs.size(), 1);
    EXPECT_EQ(tx.inputs[0].previousOutput.vout, 0);
    EXPECT_EQ(tx.inputs[0].sequence, 0xFFFFFFFF);
}

TEST_F(TransactionTest, AddOutput) {
    TxOutput output;
    output.value = 100000000;  // 1 UBU
    output.scriptPubKey = {0x76, 0xa9, 0x14};  // OP_DUP OP_HASH160 OP_PUSHBYTES_20

    tx.outputs.push_back(output);

    EXPECT_EQ(tx.outputs.size(), 1);
    EXPECT_EQ(tx.outputs[0].value, 100000000);
}

TEST_F(TransactionTest, CoinbaseDetection) {
    // Regular transaction
    TxInput input;
    input.previousOutput.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    input.previousOutput.vout = 0;
    tx.inputs.push_back(input);

    EXPECT_FALSE(tx.isCoinbase());

    // Coinbase transaction
    Transaction coinbase;
    TxInput cbInput;
    cbInput.previousOutput.txHash = Hash256();  // All zeros
    cbInput.previousOutput.vout = 0xFFFFFFFF;
    coinbase.inputs.push_back(cbInput);

    EXPECT_TRUE(coinbase.isCoinbase());
}

TEST_F(TransactionTest, TransactionHash) {
    // Add input
    TxInput input;
    input.previousOutput.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    input.previousOutput.vout = 0;
    tx.inputs.push_back(input);

    // Add output
    TxOutput output;
    output.value = 100000000;
    output.scriptPubKey = {0x76, 0xa9, 0x14};
    tx.outputs.push_back(output);

    auto hash1 = tx.getHash();
    EXPECT_NE(hash1, Hash256());

    // Same transaction should produce same hash
    auto hash2 = tx.getHash();
    EXPECT_EQ(hash1, hash2);
}

TEST_F(TransactionTest, Serialization) {
    // Create transaction
    TxInput input;
    input.previousOutput.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    input.previousOutput.vout = 0;
    input.sequence = 0xFFFFFFFF;
    tx.inputs.push_back(input);

    TxOutput output;
    output.value = 100000000;
    output.scriptPubKey = {0x76, 0xa9, 0x14};
    tx.outputs.push_back(output);

    // Serialize
    auto serialized = tx.serialize();
    EXPECT_FALSE(serialized.empty());

    // Should be deterministic
    auto serialized2 = tx.serialize();
    EXPECT_EQ(serialized, serialized2);
}

TEST_F(TransactionTest, GetSize) {
    TxInput input;
    input.previousOutput.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    input.previousOutput.vout = 0;
    tx.inputs.push_back(input);

    TxOutput output;
    output.value = 100000000;
    tx.outputs.push_back(output);

    auto size = tx.getSize();
    EXPECT_GT(size, 0);
}

// ===== TxOutpoint Tests =====

TEST(TxOutpointTest, Equality) {
    TxOutpoint out1;
    out1.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    out1.vout = 0;

    TxOutpoint out2;
    out2.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    out2.vout = 0;

    EXPECT_EQ(out1, out2);

    TxOutpoint out3;
    out3.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000002");
    out3.vout = 0;

    EXPECT_NE(out1, out3);
}

TEST(TxOutpointTest, Hash) {
    TxOutpoint out1;
    out1.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    out1.vout = 0;

    auto hash1 = std::hash<TxOutpoint>{}(out1);
    auto hash2 = std::hash<TxOutpoint>{}(out1);

    // Same outpoint should produce same hash
    EXPECT_EQ(hash1, hash2);
}

// ===== UTXO Tests =====

TEST(UTXOTest, Construction) {
    UTXO utxo;
    utxo.outpoint.txHash = Hash256::fromHex("0000000000000000000000000000000000000000000000000000000000000001");
    utxo.outpoint.vout = 0;
    utxo.output.value = 100000000;
    utxo.height = 1000;
    utxo.isCoinbase = false;

    EXPECT_EQ(utxo.outpoint.vout, 0);
    EXPECT_EQ(utxo.output.value, 100000000);
    EXPECT_EQ(utxo.height, 1000);
    EXPECT_FALSE(utxo.isCoinbase);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
