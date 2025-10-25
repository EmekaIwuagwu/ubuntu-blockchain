/**
 * Block and Transaction Validation Benchmarks
 *
 * Benchmarks for block/transaction validation and consensus operations
 */

#include "ubuntu/consensus/chainparams.h"
#include "ubuntu/consensus/pow.h"
#include "ubuntu/core/block.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/core/merkle.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"
#include "ubuntu/mempool/fee_estimator.h"
#include "ubuntu/mempool/mempool.h"

#include <benchmark/benchmark.h>
#include <random>

using namespace ubuntu;
using namespace ubuntu::core;
using namespace ubuntu::crypto;
using namespace ubuntu::consensus;

// ===== Transaction Creation Benchmarks =====

static Transaction createRandomTransaction(size_t numInputs, size_t numOutputs) {
    Transaction tx;
    tx.version = 1;
    tx.lockTime = 0;

    // Add inputs
    for (size_t i = 0; i < numInputs; ++i) {
        TxInput input;
        input.previousOutput.txHash = Hash256::fromHex(
            "0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");
        input.previousOutput.vout = static_cast<uint32_t>(i);
        input.sequence = 0xFFFFFFFF;
        input.scriptSig = {0x48, 0x30, 0x45};  // Dummy signature
        tx.inputs.push_back(input);
    }

    // Add outputs
    for (size_t i = 0; i < numOutputs; ++i) {
        TxOutput output;
        output.value = 100000000;  // 1 UBU
        output.scriptPubKey = {0x76, 0xa9, 0x14};  // P2PKH script
        tx.outputs.push_back(output);
    }

    return tx;
}

static void BM_Transaction_Create_Simple(benchmark::State& state) {
    for (auto _ : state) {
        auto tx = createRandomTransaction(1, 1);
        benchmark::DoNotOptimize(tx);
    }
}
BENCHMARK(BM_Transaction_Create_Simple);

static void BM_Transaction_Create_Complex(benchmark::State& state) {
    for (auto _ : state) {
        auto tx = createRandomTransaction(10, 10);
        benchmark::DoNotOptimize(tx);
    }
}
BENCHMARK(BM_Transaction_Create_Complex);

// ===== Transaction Serialization Benchmarks =====

static void BM_Transaction_Serialize(benchmark::State& state) {
    auto tx = createRandomTransaction(state.range(0), state.range(0));

    for (auto _ : state) {
        auto serialized = tx.serialize();
        benchmark::DoNotOptimize(serialized);
    }
}
BENCHMARK(BM_Transaction_Serialize)->Range(1, 100);

static void BM_Transaction_Hash(benchmark::State& state) {
    auto tx = createRandomTransaction(state.range(0), state.range(0));

    for (auto _ : state) {
        auto hash = tx.getHash();
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_Transaction_Hash)->Range(1, 100);

// ===== Transaction Validation Benchmarks =====

static void BM_Transaction_IsCoinbase(benchmark::State& state) {
    auto tx = createRandomTransaction(1, 1);

    for (auto _ : state) {
        bool isCoinbase = tx.isCoinbase();
        benchmark::DoNotOptimize(isCoinbase);
    }
}
BENCHMARK(BM_Transaction_IsCoinbase);

static void BM_Transaction_GetSize(benchmark::State& state) {
    auto tx = createRandomTransaction(state.range(0), state.range(0));

    for (auto _ : state) {
        auto size = tx.getSize();
        benchmark::DoNotOptimize(size);
    }
}
BENCHMARK(BM_Transaction_GetSize)->Range(1, 100);

// ===== Block Creation Benchmarks =====

static BlockHeader createRandomBlockHeader() {
    BlockHeader header;
    header.version = 1;
    header.previousBlockHash =
        Hash256::fromHex("0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");
    header.merkleRoot =
        Hash256::fromHex("1111111111111111111a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");
    header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    header.bits = 0x1d00ffff;
    header.nonce = 0;
    return header;
}

static void BM_BlockHeader_Serialize(benchmark::State& state) {
    auto header = createRandomBlockHeader();

    for (auto _ : state) {
        auto serialized = header.serialize();
        benchmark::DoNotOptimize(serialized);
    }
}
BENCHMARK(BM_BlockHeader_Serialize);

static void BM_BlockHeader_Hash(benchmark::State& state) {
    auto header = createRandomBlockHeader();

    for (auto _ : state) {
        auto hash = header.getHash();
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_BlockHeader_Hash);

// ===== Merkle Tree Benchmarks =====

static void BM_MerkleTree_Build(benchmark::State& state) {
    std::vector<Hash256> txHashes;
    for (int64_t i = 0; i < state.range(0); ++i) {
        auto tx = createRandomTransaction(1, 1);
        txHashes.push_back(tx.getHash());
    }

    for (auto _ : state) {
        auto root = MerkleTree::computeRoot(txHashes);
        benchmark::DoNotOptimize(root);
    }
}
BENCHMARK(BM_MerkleTree_Build)->Range(1, 10000);

static void BM_MerkleTree_BuildBranch(benchmark::State& state) {
    std::vector<Hash256> txHashes;
    for (int64_t i = 0; i < state.range(0); ++i) {
        auto tx = createRandomTransaction(1, 1);
        txHashes.push_back(tx.getHash());
    }

    for (auto _ : state) {
        auto branch = MerkleTree::buildMerkleBranch(txHashes, 0);
        benchmark::DoNotOptimize(branch);
    }
}
BENCHMARK(BM_MerkleTree_BuildBranch)->Range(1, 10000);

static void BM_MerkleTree_VerifyBranch(benchmark::State& state) {
    std::vector<Hash256> txHashes;
    for (int64_t i = 0; i < state.range(0); ++i) {
        auto tx = createRandomTransaction(1, 1);
        txHashes.push_back(tx.getHash());
    }

    auto root = MerkleTree::computeRoot(txHashes);
    auto branch = MerkleTree::buildMerkleBranch(txHashes, 0);

    for (auto _ : state) {
        bool valid = MerkleTree::verifyMerkleBranch(txHashes[0], branch, 0, root);
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_MerkleTree_VerifyBranch)->Range(1, 10000);

// ===== Proof of Work Benchmarks =====

static void BM_PoW_CheckWork(benchmark::State& state) {
    auto header = createRandomBlockHeader();
    auto params = ChainParams::getParams(NetworkType::MAINNET);

    for (auto _ : state) {
        bool valid = PoW::checkProofOfWork(header, params);
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_PoW_CheckWork);

static void BM_PoW_GetTarget(benchmark::State& state) {
    uint32_t bits = 0x1d00ffff;

    for (auto _ : state) {
        auto target = PoW::getTargetFromBits(bits);
        benchmark::DoNotOptimize(target);
    }
}
BENCHMARK(BM_PoW_GetTarget);

static void BM_PoW_GetBits(benchmark::State& state) {
    auto target =
        Hash256::fromHex("00000000ffff0000000000000000000000000000000000000000000000000000");

    for (auto _ : state) {
        auto bits = PoW::getBitsFromTarget(target);
        benchmark::DoNotOptimize(bits);
    }
}
BENCHMARK(BM_PoW_GetBits);

// ===== Mempool Benchmarks =====

static void BM_Mempool_AddTransaction(benchmark::State& state) {
    mempool::Mempool pool;

    int64_t counter = 0;
    for (auto _ : state) {
        state.PauseTiming();
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(counter++);  // Make each tx unique
        state.ResumeTiming();

        pool.addTransaction(tx, 50000000);
        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_Mempool_AddTransaction);

static void BM_Mempool_GetTransaction(benchmark::State& state) {
    mempool::Mempool pool;

    // Add transactions
    std::vector<Hash256> txHashes;
    for (int i = 0; i < 1000; ++i) {
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(i);
        txHashes.push_back(tx.getHash());
        pool.addTransaction(tx, 50000000);
    }

    size_t index = 0;
    for (auto _ : state) {
        auto tx = pool.getTransaction(txHashes[index % txHashes.size()]);
        benchmark::DoNotOptimize(tx);
        ++index;
    }
}
BENCHMARK(BM_Mempool_GetTransaction);

static void BM_Mempool_RemoveTransaction(benchmark::State& state) {
    int64_t counter = 0;

    for (auto _ : state) {
        state.PauseTiming();
        mempool::Mempool pool;
        std::vector<Hash256> txHashes;
        for (int i = 0; i < 100; ++i) {
            auto tx = createRandomTransaction(1, 1);
            tx.lockTime = static_cast<uint32_t>(counter++);
            txHashes.push_back(tx.getHash());
            pool.addTransaction(tx, 50000000);
        }
        state.ResumeTiming();

        for (const auto& hash : txHashes) {
            pool.removeTransaction(hash);
        }
        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_Mempool_RemoveTransaction);

static void BM_Mempool_GetStats(benchmark::State& state) {
    mempool::Mempool pool;

    // Add transactions
    for (int i = 0; i < 1000; ++i) {
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(i);
        pool.addTransaction(tx, 50000000);
    }

    for (auto _ : state) {
        auto stats = pool.getStats();
        benchmark::DoNotOptimize(stats);
    }
}
BENCHMARK(BM_Mempool_GetStats);

// ===== Fee Estimation Benchmarks =====

static void BM_FeeEstimator_AddSample(benchmark::State& state) {
    mempool::FeeEstimator estimator;

    int64_t counter = 0;
    for (auto _ : state) {
        estimator.addConfirmedTransaction(1000 + counter, 1 + (counter % 10));
        benchmark::DoNotOptimize(estimator);
        ++counter;
    }
}
BENCHMARK(BM_FeeEstimator_AddSample);

static void BM_FeeEstimator_Estimate(benchmark::State& state) {
    mempool::FeeEstimator estimator;

    // Add samples
    for (int i = 0; i < 1000; ++i) {
        estimator.addConfirmedTransaction(1000 + i, 1 + (i % 20));
    }

    for (auto _ : state) {
        auto feeRate = estimator.estimateFee(6);
        benchmark::DoNotOptimize(feeRate);
    }
}
BENCHMARK(BM_FeeEstimator_Estimate);

// ===== Block Validation Benchmarks =====

static void BM_Block_Validate_Small(benchmark::State& state) {
    Block block;
    block.header = createRandomBlockHeader();

    // Add coinbase
    auto coinbase = createRandomTransaction(1, 1);
    block.transactions.push_back(coinbase);

    // Add 10 transactions
    for (int i = 0; i < 10; ++i) {
        block.transactions.push_back(createRandomTransaction(2, 2));
    }

    for (auto _ : state) {
        auto size = block.getSize();
        benchmark::DoNotOptimize(size);
    }
}
BENCHMARK(BM_Block_Validate_Small);

static void BM_Block_Validate_Large(benchmark::State& state) {
    Block block;
    block.header = createRandomBlockHeader();

    // Add coinbase
    auto coinbase = createRandomTransaction(1, 1);
    block.transactions.push_back(coinbase);

    // Add 1000 transactions
    for (int i = 0; i < 1000; ++i) {
        block.transactions.push_back(createRandomTransaction(2, 2));
    }

    for (auto _ : state) {
        auto size = block.getSize();
        benchmark::DoNotOptimize(size);
    }
}
BENCHMARK(BM_Block_Validate_Large);

static void BM_Block_ComputeMerkleRoot(benchmark::State& state) {
    Block block;
    block.header = createRandomBlockHeader();

    // Add transactions
    for (int64_t i = 0; i < state.range(0); ++i) {
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(i);
        block.transactions.push_back(tx);
    }

    for (auto _ : state) {
        std::vector<Hash256> txHashes;
        for (const auto& tx : block.transactions) {
            txHashes.push_back(tx.getHash());
        }
        auto root = MerkleTree::computeRoot(txHashes);
        benchmark::DoNotOptimize(root);
    }
}
BENCHMARK(BM_Block_ComputeMerkleRoot)->Range(1, 10000);

BENCHMARK_MAIN();
