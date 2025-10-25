#include "ubuntu/metrics/metrics.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>

namespace ubuntu {
namespace metrics {

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

MetricsCollector& MetricsCollector::getInstance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::incrementCounter(const std::string& name, int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_[name].fetch_add(value);
}

void MetricsCollector::setGauge(const std::string& name, int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    gauges_[name].store(value);
}

void MetricsCollector::recordTiming(const std::string& name, int64_t durationMs) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& timing = timings_[name];
    timing.count++;
    timing.total += durationMs;
    timing.min = std::min(timing.min, durationMs);
    timing.max = std::max(timing.max, durationMs);
}

int64_t MetricsCollector::getCounter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(name);
    if (it != counters_.end()) {
        return it->second.load();
    }
    return 0;
}

int64_t MetricsCollector::getGauge(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gauges_.find(name);
    if (it != gauges_.end()) {
        return it->second.load();
    }
    return 0;
}

std::map<std::string, int64_t> MetricsCollector::getTimingStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, int64_t> stats;

    auto it = timings_.find(name);
    if (it != timings_.end()) {
        const auto& timing = it->second;
        stats["count"] = timing.count;
        stats["total"] = timing.total;
        stats["avg"] = timing.count > 0 ? (timing.total / timing.count) : 0;
        stats["min"] = timing.min == std::numeric_limits<int64_t>::max() ? 0 : timing.min;
        stats["max"] = timing.max;
    }

    return stats;
}

std::string MetricsCollector::getPrometheusMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    // Counters
    for (const auto& [name, value] : counters_) {
        oss << "# TYPE ubuntu_" << name << " counter\n";
        oss << "ubuntu_" << name << " " << value.load() << "\n";
    }

    // Gauges
    for (const auto& [name, value] : gauges_) {
        oss << "# TYPE ubuntu_" << name << " gauge\n";
        oss << "ubuntu_" << name << " " << value.load() << "\n";
    }

    // Timings
    for (const auto& [name, timing] : timings_) {
        oss << "# TYPE ubuntu_" << name << "_count counter\n";
        oss << "ubuntu_" << name << "_count " << timing.count << "\n";

        oss << "# TYPE ubuntu_" << name << "_total counter\n";
        oss << "ubuntu_" << name << "_total " << timing.total << "\n";

        if (timing.count > 0) {
            int64_t avg = timing.total / timing.count;
            oss << "# TYPE ubuntu_" << name << "_avg gauge\n";
            oss << "ubuntu_" << name << "_avg " << avg << "\n";
        }
    }

    return oss.str();
}

std::string MetricsCollector::getJsonMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    oss << "{\n";

    // Counters
    oss << "  \"counters\": {\n";
    bool first = true;
    for (const auto& [name, value] : counters_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    \"" << name << "\": " << value.load();
    }
    oss << "\n  },\n";

    // Gauges
    oss << "  \"gauges\": {\n";
    first = true;
    for (const auto& [name, value] : gauges_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    \"" << name << "\": " << value.load();
    }
    oss << "\n  },\n";

    // Timings
    oss << "  \"timings\": {\n";
    first = true;
    for (const auto& [name, timing] : timings_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    \"" << name << "\": {\n";
        oss << "      \"count\": " << timing.count << ",\n";
        oss << "      \"total\": " << timing.total << ",\n";

        if (timing.count > 0) {
            int64_t avg = timing.total / timing.count;
            oss << "      \"avg\": " << avg << ",\n";
        } else {
            oss << "      \"avg\": 0,\n";
        }

        oss << "      \"min\": " << (timing.min == std::numeric_limits<int64_t>::max() ? 0 : timing.min) << ",\n";
        oss << "      \"max\": " << timing.max << "\n";
        oss << "    }";
    }
    oss << "\n  }\n";

    oss << "}\n";

    return oss.str();
}

void MetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.clear();
    gauges_.clear();
    timings_.clear();
    spdlog::info("Metrics reset");
}

// ============================================================================
// ScopedTimer Implementation
// ============================================================================

ScopedTimer::ScopedTimer(const std::string& name)
    : name_(name), start_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    MetricsCollector::getInstance().recordTiming(name_, duration.count());
}

}  // namespace metrics
}  // namespace ubuntu
