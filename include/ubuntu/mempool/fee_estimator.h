#pragma once

#include "ubuntu/core/block.h"
#include "ubuntu/core/transaction.h"

#include <map>
#include <optional>
#include <vector>

namespace ubuntu {
namespace mempool {

/**
 * @brief Fee rate estimator
 *
 * Estimates appropriate fee rates for transactions based on recent block history.
 * Uses a statistical approach to predict confirmation times for different fee rates.
 */
class FeeEstimator {
public:
    /**
     * @brief Construct fee estimator
     */
    FeeEstimator();
    ~FeeEstimator();

    /**
     * @brief Process a confirmed block
     *
     * Analyzes transactions in the block to update fee rate statistics.
     *
     * @param block Confirmed block
     * @param height Block height
     */
    void processBlock(const core::Block& block, uint32_t height);

    /**
     * @brief Estimate fee rate for target confirmation time
     *
     * @param targetBlocks Target number of blocks for confirmation (e.g., 1, 3, 6)
     * @return Estimated fee rate in satoshis per byte, or nullopt if insufficient data
     */
    std::optional<uint64_t> estimateFeeRate(uint32_t targetBlocks) const;

    /**
     * @brief Estimate fee for a transaction
     *
     * @param tx Transaction to estimate fee for
     * @param targetBlocks Target confirmation time in blocks
     * @return Estimated fee in satoshis
     */
    std::optional<uint64_t> estimateFee(const core::Transaction& tx,
                                         uint32_t targetBlocks) const;

    /**
     * @brief Get fee rate statistics
     */
    struct FeeStats {
        uint64_t minFeeRate;      // Minimum observed fee rate
        uint64_t maxFeeRate;      // Maximum observed fee rate
        uint64_t medianFeeRate;   // Median fee rate
        uint64_t avgFeeRate;      // Average fee rate
        size_t sampleSize;        // Number of transactions sampled
    };

    /**
     * @brief Get statistics for a confirmation target
     *
     * @param targetBlocks Target confirmation time
     * @return Fee statistics if available
     */
    std::optional<FeeStats> getStats(uint32_t targetBlocks) const;

    /**
     * @brief Clear all historical data
     */
    void clear();

private:
    /**
     * @brief Transaction fee rate record
     */
    struct FeeRecord {
        uint64_t feeRate;       // Fee rate in sat/byte
        uint32_t blockHeight;   // Block where tx was confirmed
        uint32_t blocksWaited;  // Number of blocks tx waited in mempool
    };

    // Historical fee rate data
    std::vector<FeeRecord> history_;

    // Statistics cache per confirmation target
    mutable std::map<uint32_t, FeeStats> statsCache_;
    mutable bool statsCacheDirty_;

    // Configuration
    static constexpr size_t MAX_HISTORY_SIZE = 10000;
    static constexpr size_t MIN_SAMPLE_SIZE = 100;
    static constexpr double CONFIRMATION_TARGET_MULTIPLIER = 1.2;  // 20% safety margin

    /**
     * @brief Update statistics cache
     */
    void updateStatsCache() const;

    /**
     * @brief Calculate statistics for a target
     */
    FeeStats calculateStats(uint32_t targetBlocks) const;

    /**
     * @brief Get fee rates for transactions confirmed within target blocks
     */
    std::vector<uint64_t> getFeeRatesForTarget(uint32_t targetBlocks) const;

    /**
     * @brief Calculate median of a sorted vector
     */
    static uint64_t calculateMedian(const std::vector<uint64_t>& values);

    /**
     * @brief Calculate average of a vector
     */
    static uint64_t calculateAverage(const std::vector<uint64_t>& values);

    /**
     * @brief Prune old history entries
     */
    void pruneHistory();
};

}  // namespace mempool
}  // namespace ubuntu
