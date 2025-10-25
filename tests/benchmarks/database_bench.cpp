#include <benchmark/benchmark.h>

static void BM_Database(benchmark::State& state) {
    for (auto _ : state) {
        // Placeholder
    }
}
BENCHMARK(BM_Database);

BENCHMARK_MAIN();
