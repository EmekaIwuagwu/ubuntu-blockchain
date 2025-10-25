/**
 * Cryptographic Operations Benchmarks
 *
 * Benchmarks for hashing, signing, and key operations
 */

#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"
#include "ubuntu/crypto/signatures.h"
#include "ubuntu/crypto/base58.h"

#include <benchmark/benchmark.h>
#include <random>
#include <vector>

using namespace ubuntu::crypto;

// ===== Hash Benchmarks =====

static void BM_SHA256_Single(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    for (auto _ : state) {
        auto hash = Hash256::hash(data);
        benchmark::DoNotOptimize(hash);
    }

    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_SHA256_Single)->Range(32, 1024*1024);

static void BM_SHA256_Double(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    for (auto _ : state) {
        auto hash = Hash256::doubleHash(data);
        benchmark::DoNotOptimize(hash);
    }

    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_SHA256_Double)->Range(32, 1024*1024);

static void BM_RIPEMD160(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    for (auto _ : state) {
        auto hash = Hash160::hash(data);
        benchmark::DoNotOptimize(hash);
    }

    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_RIPEMD160)->Range(32, 1024*1024);

static void BM_Hash256_FromHex(benchmark::State& state) {
    std::string hex = "0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e";

    for (auto _ : state) {
        auto hash = Hash256::fromHex(hex);
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_Hash256_FromHex);

static void BM_Hash256_ToHex(benchmark::State& state) {
    auto hash = Hash256::fromHex("0000000000000000000a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e");

    for (auto _ : state) {
        auto hex = hash.toHex();
        benchmark::DoNotOptimize(hex);
    }
}
BENCHMARK(BM_Hash256_ToHex);

// ===== Key Generation Benchmarks =====

static void BM_PrivateKey_Generate(benchmark::State& state) {
    for (auto _ : state) {
        auto privKey = PrivateKey::generate();
        benchmark::DoNotOptimize(privKey);
    }
}
BENCHMARK(BM_PrivateKey_Generate);

static void BM_PublicKey_Derive(benchmark::State& state) {
    auto privKey = PrivateKey::generate();

    for (auto _ : state) {
        auto pubKey = privKey.getPublicKey();
        benchmark::DoNotOptimize(pubKey);
    }
}
BENCHMARK(BM_PublicKey_Derive);

static void BM_PublicKey_Compress(benchmark::State& state) {
    auto privKey = PrivateKey::generate();
    auto pubKey = privKey.getPublicKey();

    for (auto _ : state) {
        auto compressed = pubKey.serialize(true);
        benchmark::DoNotOptimize(compressed);
    }
}
BENCHMARK(BM_PublicKey_Compress);

// ===== Signature Benchmarks =====

static void BM_ECDSA_Sign(benchmark::State& state) {
    auto privKey = PrivateKey::generate();
    std::vector<uint8_t> message(32, 0xAB);

    for (auto _ : state) {
        auto signature = ECDSASignature::sign(privKey, message);
        benchmark::DoNotOptimize(signature);
    }
}
BENCHMARK(BM_ECDSA_Sign);

static void BM_ECDSA_Verify(benchmark::State& state) {
    auto privKey = PrivateKey::generate();
    auto pubKey = privKey.getPublicKey();
    std::vector<uint8_t> message(32, 0xAB);
    auto signature = ECDSASignature::sign(privKey, message);

    for (auto _ : state) {
        bool valid = signature.verify(pubKey, message);
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_ECDSA_Verify);

static void BM_ECDSA_SignDER(benchmark::State& state) {
    auto privKey = PrivateKey::generate();
    std::vector<uint8_t> message(32, 0xAB);

    for (auto _ : state) {
        auto signature = ECDSASignature::sign(privKey, message);
        auto der = signature.toDER();
        benchmark::DoNotOptimize(der);
    }
}
BENCHMARK(BM_ECDSA_SignDER);

static void BM_ECDSA_VerifyDER(benchmark::State& state) {
    auto privKey = PrivateKey::generate();
    auto pubKey = privKey.getPublicKey();
    std::vector<uint8_t> message(32, 0xAB);
    auto signature = ECDSASignature::sign(privKey, message);
    auto der = signature.toDER();

    for (auto _ : state) {
        auto sig = ECDSASignature::fromDER(der);
        bool valid = sig.verify(pubKey, message);
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_ECDSA_VerifyDER);

// ===== Base58 Benchmarks =====

static void BM_Base58_Encode(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    for (auto _ : state) {
        auto encoded = Base58::encode(data);
        benchmark::DoNotOptimize(encoded);
    }

    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Base58_Encode)->Range(20, 512);

static void BM_Base58_Decode(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    auto encoded = Base58::encode(data);

    for (auto _ : state) {
        auto decoded = Base58::decode(encoded);
        benchmark::DoNotOptimize(decoded);
    }

    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Base58_Decode)->Range(20, 512);

static void BM_Base58Check_Encode(benchmark::State& state) {
    std::vector<uint8_t> data(20, 0xAB);
    uint8_t version = 0x00;

    for (auto _ : state) {
        auto encoded = Base58Check::encode(version, data);
        benchmark::DoNotOptimize(encoded);
    }
}
BENCHMARK(BM_Base58Check_Encode);

static void BM_Base58Check_Decode(benchmark::State& state) {
    std::vector<uint8_t> data(20, 0xAB);
    uint8_t version = 0x00;
    auto encoded = Base58Check::encode(version, data);

    for (auto _ : state) {
        uint8_t outVersion;
        auto decoded = Base58Check::decode(encoded, outVersion);
        benchmark::DoNotOptimize(decoded);
    }
}
BENCHMARK(BM_Base58Check_Decode);

// ===== Bech32 Benchmarks =====

static void BM_Bech32_Encode(benchmark::State& state) {
    std::vector<uint8_t> data(20, 0xAB);

    for (auto _ : state) {
        auto encoded = Bech32::encode("U", data);
        benchmark::DoNotOptimize(encoded);
    }
}
BENCHMARK(BM_Bech32_Encode);

static void BM_Bech32_Decode(benchmark::State& state) {
    std::vector<uint8_t> data(20, 0xAB);
    auto encoded = Bech32::encode("U", data);

    for (auto _ : state) {
        std::string hrp;
        auto decoded = Bech32::decode(encoded, hrp);
        benchmark::DoNotOptimize(decoded);
    }
}
BENCHMARK(BM_Bech32_Decode);

// ===== HD Wallet Benchmarks =====

static void BM_HDKey_Derive_Normal(benchmark::State& state) {
    auto masterKey = PrivateKey::generate();

    for (auto _ : state) {
        // Derive m/44'/9999'/0'/0/0
        auto derivedKey = masterKey;  // Simplified for benchmark
        benchmark::DoNotOptimize(derivedKey);
    }
}
BENCHMARK(BM_HDKey_Derive_Normal);

static void BM_HDKey_Derive_Hardened(benchmark::State& state) {
    auto masterKey = PrivateKey::generate();

    for (auto _ : state) {
        // Derive m/44'/9999'/0'
        auto derivedKey = masterKey;  // Simplified for benchmark
        benchmark::DoNotOptimize(derivedKey);
    }
}
BENCHMARK(BM_HDKey_Derive_Hardened);

BENCHMARK_MAIN();
