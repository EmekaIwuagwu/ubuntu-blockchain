#include <benchmark/benchmark.h>

static void BM_Validation(benchmark::State& state) {
    for (auto _ : state) {
        // Placeholder
    }
}
BENCHMARK(BM_Validation);

BENCHMARK_MAIN();
