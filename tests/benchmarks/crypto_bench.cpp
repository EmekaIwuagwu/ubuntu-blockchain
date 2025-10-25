#include "ubuntu/crypto/hash.h"
#include <benchmark/benchmark.h>

static void BM_SHA256(benchmark::State& state) {
    std::string data = "Ubuntu Blockchain";
    for (auto _ : state) {
        auto hash = ubuntu::crypto::sha256(data);
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(BM_SHA256);

BENCHMARK_MAIN();
