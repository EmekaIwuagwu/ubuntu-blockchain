#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ubuntu {
namespace metrics {

/**
 * @brief Metrics collector
 *
 * Collects and tracks performance metrics for monitoring.
 */
class MetricsCollector {
public:
    /**
     * @brief Get singleton instance
     *
     * @return MetricsCollector instance
     */
    static MetricsCollector& getInstance();

    /**
     * @brief Increment a counter
     *
     * @param name Counter name
     * @param value Value to add (default: 1)
     */
    void incrementCounter(const std::string& name, int64_t value = 1);

    /**
     * @brief Set a gauge value
     *
     * @param name Gauge name
     * @param value Value to set
     */
    void setGauge(const std::string& name, int64_t value);

    /**
     * @brief Record a timing measurement
     *
     * @param name Timer name
     * @param durationMs Duration in milliseconds
     */
    void recordTiming(const std::string& name, int64_t durationMs);

    /**
     * @brief Get counter value
     *
     * @param name Counter name
     * @return Current value
     */
    int64_t getCounter(const std::string& name) const;

    /**
     * @brief Get gauge value
     *
     * @param name Gauge name
     * @return Current value
     */
    int64_t getGauge(const std::string& name) const;

    /**
     * @brief Get timing statistics
     *
     * @param name Timer name
     * @return Map of statistics (count, total, avg, min, max)
     */
    std::map<std::string, int64_t> getTimingStats(const std::string& name) const;

    /**
     * @brief Get all metrics as Prometheus format
     *
     * @return Metrics in Prometheus text format
     */
    std::string getPrometheusMetrics() const;

    /**
     * @brief Get all metrics as JSON
     *
     * @return Metrics in JSON format
     */
    std::string getJsonMetrics() const;

    /**
     * @brief Reset all metrics
     */
    void reset();

private:
    MetricsCollector() = default;
    ~MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    struct TimingData {
        int64_t count = 0;
        int64_t total = 0;
        int64_t min = std::numeric_limits<int64_t>::max();
        int64_t max = 0;
    };

    mutable std::mutex mutex_;
    std::map<std::string, std::atomic<int64_t>> counters_;
    std::map<std::string, std::atomic<int64_t>> gauges_;
    std::map<std::string, TimingData> timings_;
};

/**
 * @brief RAII timer for automatic timing measurements
 */
class ScopedTimer {
public:
    /**
     * @brief Start timer
     *
     * @param name Timer name
     */
    explicit ScopedTimer(const std::string& name);

    /**
     * @brief Stop timer and record measurement
     */
    ~ScopedTimer();

private:
    std::string name_;
    std::chrono::steady_clock::time_point start_;
};

// Convenience macros
#define METRICS_COUNTER(name, value) \
    ubuntu::metrics::MetricsCollector::getInstance().incrementCounter(name, value)

#define METRICS_GAUGE(name, value) \
    ubuntu::metrics::MetricsCollector::getInstance().setGauge(name, value)

#define METRICS_TIMER(name) ubuntu::metrics::ScopedTimer _timer_##__LINE__(name)

}  // namespace metrics
}  // namespace ubuntu
