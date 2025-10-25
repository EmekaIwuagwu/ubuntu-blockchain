/**
 * Database Operations Benchmarks
 *
 * Benchmarks for RocksDB storage, UTXO database, and block index operations
 */

#include "ubuntu/core/block.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/storage/block_index.h"
#include "ubuntu/storage/database.h"
#include "ubuntu/storage/utxo_db.h"

#include <benchmark/benchmark.h>
#include <filesystem>
#include <random>

using namespace ubuntu;
using namespace ubuntu::core;
using namespace ubuntu::crypto;
using namespace ubuntu::storage;

namespace fs = std::filesystem;

// ===== Helper Functions =====

static std::string getTempDbPath() {
    static int counter = 0;
    return "/tmp/ubu_bench_db_" + std::to_string(counter++);
}

static Transaction createRandomTransaction(size_t numInputs, size_t numOutputs) {
    Transaction tx;
    tx.version = 1;
    tx.lockTime = 0;

    for (size_t i = 0; i < numInputs; ++i) {
        TxInput input;
        input.previousOutput.txHash = Hash256::fromHex(
            "0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");
        input.previousOutput.vout = static_cast<uint32_t>(i);
        input.sequence = 0xFFFFFFFF;
        input.scriptSig = {0x48, 0x30, 0x45};
        tx.inputs.push_back(input);
    }

    for (size_t i = 0; i < numOutputs; ++i) {
        TxOutput output;
        output.value = 100000000;  // 1 UBU
        output.scriptPubKey = {0x76, 0xa9, 0x14};
        tx.outputs.push_back(output);
    }

    return tx;
}

static Block createRandomBlock(size_t numTransactions) {
    Block block;
    block.header.version = 1;
    block.header.previousBlockHash =
        Hash256::fromHex("0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");
    block.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    block.header.bits = 0x1d00ffff;
    block.header.nonce = 0;

    // Add coinbase
    block.transactions.push_back(createRandomTransaction(1, 1));

    // Add regular transactions
    for (size_t i = 1; i < numTransactions; ++i) {
        block.transactions.push_back(createRandomTransaction(2, 2));
    }

    return block;
}

// ===== Database Benchmarks =====

static void BM_Database_Open(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto dbPath = getTempDbPath();
        state.ResumeTiming();

        auto db = std::make_shared<Database>(dbPath);
        benchmark::DoNotOptimize(db);

        state.PauseTiming();
        db.reset();
        fs::remove_all(dbPath);
        state.ResumeTiming();
    }
}
BENCHMARK(BM_Database_Open);

static void BM_Database_Put(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);

    std::vector<uint8_t> value(state.range(0), 0xAB);
    int64_t counter = 0;

    for (auto _ : state) {
        std::string key = "key_" + std::to_string(counter++);
        db->put("default", key, value);
    }

    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_Database_Put)->Range(64, 1024*1024);

static void BM_Database_Get(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);

    // Pre-populate database
    std::vector<uint8_t> value(1024, 0xAB);
    std::vector<std::string> keys;
    for (int i = 0; i < 10000; ++i) {
        std::string key = "key_" + std::to_string(i);
        db->put("default", key, value);
        keys.push_back(key);
    }

    size_t index = 0;
    for (auto _ : state) {
        auto result = db->get("default", keys[index % keys.size()]);
        benchmark::DoNotOptimize(result);
        ++index;
    }

    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_Database_Get);

static void BM_Database_Delete(benchmark::State& state) {
    auto dbPath = getTempDbPath();

    for (auto _ : state) {
        state.PauseTiming();
        auto db = std::make_shared<Database>(dbPath);
        std::vector<std::string> keys;
        std::vector<uint8_t> value(1024, 0xAB);
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(i);
            db->put("default", key, value);
            keys.push_back(key);
        }
        state.ResumeTiming();

        for (const auto& key : keys) {
            db->remove("default", key);
        }
        benchmark::DoNotOptimize(db);

        state.PauseTiming();
        db.reset();
        state.ResumeTiming();
    }

    fs::remove_all(dbPath);
}
BENCHMARK(BM_Database_Delete);

static void BM_Database_Batch_Write(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);

    std::vector<uint8_t> value(1024, 0xAB);

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
        for (int64_t i = 0; i < state.range(0); ++i) {
            batch.emplace_back("key_" + std::to_string(i), value);
        }
        state.ResumeTiming();

        for (const auto& [key, val] : batch) {
            db->put("default", key, val);
        }
        benchmark::DoNotOptimize(db);
    }

    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_Database_Batch_Write)->Range(10, 10000);

static void BM_Database_Iterator(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);

    // Pre-populate
    std::vector<uint8_t> value(128, 0xAB);
    for (int64_t i = 0; i < state.range(0); ++i) {
        db->put("default", "key_" + std::to_string(i), value);
    }

    for (auto _ : state) {
        auto it = db->newIterator("default");
        int count = 0;
        for (it->seekToFirst(); it->valid(); it->next()) {
            ++count;
        }
        benchmark::DoNotOptimize(count);
    }

    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_Database_Iterator)->Range(100, 100000);

// ===== UTXO Database Benchmarks =====

static void BM_UTXODatabase_AddUTXO(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto utxoDb = std::make_shared<UTXODatabase>(db);

    int64_t counter = 0;
    for (auto _ : state) {
        state.PauseTiming();
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(counter);
        TxOutpoint outpoint;
        outpoint.txHash = tx.getHash();
        outpoint.vout = 0;
        UTXO utxo;
        utxo.outpoint = outpoint;
        utxo.output = tx.outputs[0];
        utxo.height = counter;
        utxo.isCoinbase = false;
        ++counter;
        state.ResumeTiming();

        utxoDb->addUTXO(utxo);
        benchmark::DoNotOptimize(utxoDb);
    }

    utxoDb.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_UTXODatabase_AddUTXO);

static void BM_UTXODatabase_GetUTXO(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto utxoDb = std::make_shared<UTXODatabase>(db);

    // Pre-populate
    std::vector<TxOutpoint> outpoints;
    for (int i = 0; i < 10000; ++i) {
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(i);
        TxOutpoint outpoint;
        outpoint.txHash = tx.getHash();
        outpoint.vout = 0;
        UTXO utxo;
        utxo.outpoint = outpoint;
        utxo.output = tx.outputs[0];
        utxo.height = i;
        utxo.isCoinbase = false;
        utxoDb->addUTXO(utxo);
        outpoints.push_back(outpoint);
    }

    size_t index = 0;
    for (auto _ : state) {
        auto utxo = utxoDb->getUTXO(outpoints[index % outpoints.size()]);
        benchmark::DoNotOptimize(utxo);
        ++index;
    }

    utxoDb.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_UTXODatabase_GetUTXO);

static void BM_UTXODatabase_RemoveUTXO(benchmark::State& state) {
    auto dbPath = getTempDbPath();

    for (auto _ : state) {
        state.PauseTiming();
        auto db = std::make_shared<Database>(dbPath);
        auto utxoDb = std::make_shared<UTXODatabase>(db);
        std::vector<TxOutpoint> outpoints;
        for (int i = 0; i < 1000; ++i) {
            auto tx = createRandomTransaction(1, 1);
            tx.lockTime = static_cast<uint32_t>(i);
            TxOutpoint outpoint;
            outpoint.txHash = tx.getHash();
            outpoint.vout = 0;
            UTXO utxo;
            utxo.outpoint = outpoint;
            utxo.output = tx.outputs[0];
            utxo.height = i;
            utxo.isCoinbase = false;
            utxoDb->addUTXO(utxo);
            outpoints.push_back(outpoint);
        }
        state.ResumeTiming();

        for (const auto& outpoint : outpoints) {
            utxoDb->removeUTXO(outpoint);
        }
        benchmark::DoNotOptimize(utxoDb);

        state.PauseTiming();
        utxoDb.reset();
        db.reset();
        state.ResumeTiming();
    }

    fs::remove_all(dbPath);
}
BENCHMARK(BM_UTXODatabase_RemoveUTXO);

static void BM_UTXODatabase_HasUTXO(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto utxoDb = std::make_shared<UTXODatabase>(db);

    // Pre-populate
    std::vector<TxOutpoint> outpoints;
    for (int i = 0; i < 10000; ++i) {
        auto tx = createRandomTransaction(1, 1);
        tx.lockTime = static_cast<uint32_t>(i);
        TxOutpoint outpoint;
        outpoint.txHash = tx.getHash();
        outpoint.vout = 0;
        UTXO utxo;
        utxo.outpoint = outpoint;
        utxo.output = tx.outputs[0];
        utxo.height = i;
        utxo.isCoinbase = false;
        utxoDb->addUTXO(utxo);
        outpoints.push_back(outpoint);
    }

    size_t index = 0;
    for (auto _ : state) {
        bool exists = utxoDb->hasUTXO(outpoints[index % outpoints.size()]);
        benchmark::DoNotOptimize(exists);
        ++index;
    }

    utxoDb.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_UTXODatabase_HasUTXO);

static void BM_UTXODatabase_Flush(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto utxoDb = std::make_shared<UTXODatabase>(db);

    for (auto _ : state) {
        state.PauseTiming();
        // Add UTXOs
        for (int64_t i = 0; i < state.range(0); ++i) {
            auto tx = createRandomTransaction(1, 1);
            tx.lockTime = static_cast<uint32_t>(i);
            TxOutpoint outpoint;
            outpoint.txHash = tx.getHash();
            outpoint.vout = 0;
            UTXO utxo;
            utxo.outpoint = outpoint;
            utxo.output = tx.outputs[0];
            utxo.height = i;
            utxo.isCoinbase = false;
            utxoDb->addUTXO(utxo);
        }
        state.ResumeTiming();

        utxoDb->flush();
        benchmark::DoNotOptimize(utxoDb);
    }

    utxoDb.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_UTXODatabase_Flush)->Range(100, 10000);

// ===== Block Storage Benchmarks =====

static void BM_BlockStorage_WriteBlock(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockStorage = std::make_shared<BlockStorage>(db);

    int64_t counter = 0;
    for (auto _ : state) {
        state.PauseTiming();
        auto block = createRandomBlock(state.range(0));
        block.header.nonce = static_cast<uint32_t>(counter++);
        state.ResumeTiming();

        blockStorage->writeBlock(block);
        benchmark::DoNotOptimize(blockStorage);
    }

    blockStorage.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockStorage_WriteBlock)->Range(1, 1000);

static void BM_BlockStorage_ReadBlock(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockStorage = std::make_shared<BlockStorage>(db);

    // Pre-populate
    std::vector<Hash256> blockHashes;
    for (int i = 0; i < 100; ++i) {
        auto block = createRandomBlock(10);
        block.header.nonce = static_cast<uint32_t>(i);
        blockStorage->writeBlock(block);
        blockHashes.push_back(block.header.getHash());
    }

    size_t index = 0;
    for (auto _ : state) {
        auto block = blockStorage->readBlock(blockHashes[index % blockHashes.size()]);
        benchmark::DoNotOptimize(block);
        ++index;
    }

    blockStorage.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockStorage_ReadBlock);

static void BM_BlockStorage_HasBlock(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockStorage = std::make_shared<BlockStorage>(db);

    // Pre-populate
    std::vector<Hash256> blockHashes;
    for (int i = 0; i < 100; ++i) {
        auto block = createRandomBlock(10);
        block.header.nonce = static_cast<uint32_t>(i);
        blockStorage->writeBlock(block);
        blockHashes.push_back(block.header.getHash());
    }

    size_t index = 0;
    for (auto _ : state) {
        bool exists = blockStorage->hasBlock(blockHashes[index % blockHashes.size()]);
        benchmark::DoNotOptimize(exists);
        ++index;
    }

    blockStorage.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockStorage_HasBlock);

// ===== Block Index Benchmarks =====

static void BM_BlockIndex_AddBlock(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockIndex = std::make_shared<BlockIndex>(db);

    int64_t counter = 0;
    for (auto _ : state) {
        state.PauseTiming();
        auto block = createRandomBlock(1);
        block.header.nonce = static_cast<uint32_t>(counter++);
        BlockIndexEntry entry;
        entry.hash = block.header.getHash();
        entry.height = counter;
        entry.header = block.header;
        entry.chainWork = Hash256();
        state.ResumeTiming();

        blockIndex->addBlock(entry);
        benchmark::DoNotOptimize(blockIndex);
    }

    blockIndex.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockIndex_AddBlock);

static void BM_BlockIndex_GetBlock(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockIndex = std::make_shared<BlockIndex>(db);

    // Pre-populate
    std::vector<Hash256> blockHashes;
    for (int i = 0; i < 1000; ++i) {
        auto block = createRandomBlock(1);
        block.header.nonce = static_cast<uint32_t>(i);
        BlockIndexEntry entry;
        entry.hash = block.header.getHash();
        entry.height = i;
        entry.header = block.header;
        entry.chainWork = Hash256();
        blockIndex->addBlock(entry);
        blockHashes.push_back(entry.hash);
    }

    size_t index = 0;
    for (auto _ : state) {
        auto entry = blockIndex->getBlock(blockHashes[index % blockHashes.size()]);
        benchmark::DoNotOptimize(entry);
        ++index;
    }

    blockIndex.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockIndex_GetBlock);

static void BM_BlockIndex_GetBlockByHeight(benchmark::State& state) {
    auto dbPath = getTempDbPath();
    auto db = std::make_shared<Database>(dbPath);
    auto blockIndex = std::make_shared<BlockIndex>(db);

    // Pre-populate
    for (int i = 0; i < 1000; ++i) {
        auto block = createRandomBlock(1);
        block.header.nonce = static_cast<uint32_t>(i);
        BlockIndexEntry entry;
        entry.hash = block.header.getHash();
        entry.height = i;
        entry.header = block.header;
        entry.chainWork = Hash256();
        blockIndex->addBlock(entry);
    }

    size_t height = 0;
    for (auto _ : state) {
        auto entry = blockIndex->getBlockByHeight(height % 1000);
        benchmark::DoNotOptimize(entry);
        ++height;
    }

    blockIndex.reset();
    db.reset();
    fs::remove_all(dbPath);
}
BENCHMARK(BM_BlockIndex_GetBlockByHeight);

BENCHMARK_MAIN();
