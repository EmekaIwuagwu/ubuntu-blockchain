#include "ubuntu/mempool/fee_estimator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <numeric>

namespace ubuntu {
namespace mempool {

// ============================================================================
// FeeEstimator Implementation
// ============================================================================

FeeEstimator::FeeEstimator() : statsCacheDirty_(true) {}

FeeEstimator::~FeeEstimator() = default;

void FeeEstimator::processBlock(const core::Block& block, uint32_t height) {
    // Process all transactions in block (skip coinbase)
    for (size_t i = 1; i < block.transactions.size(); ++i) {
        const auto& tx = block.transactions[i];

        // Calculate fee rate
        // In a full implementation, we would track when the transaction
        // entered the mempool to know how many blocks it waited
        uint64_t txSize = tx.getSize();
        if (txSize == 0) {
            continue;
        }

        // For now, we'll estimate fee based on transaction structure
        // In production, this would come from UTXO lookups
        uint64_t estimatedFee = 0;
        for (const auto& input : tx.inputs) {
            // Assume average input value (this is a simplification)
            estimatedFee += 1000;  // Placeholder
        }

        for (const auto& output : tx.outputs) {
            if (estimatedFee > output.value) {
                estimatedFee -= output.value;
            }
        }

        uint64_t feeRate = estimatedFee / txSize;

        // Add to history
        FeeRecord record;
        record.feeRate = feeRate;
        record.blockHeight = height;
        record.blocksWaited = 1;  // Simplified: assume 1 block wait

        history_.push_back(record);
    }

    // Prune old entries
    if (history_.size() > MAX_HISTORY_SIZE) {
        pruneHistory();
    }

    // Invalidate cache
    statsCacheDirty_ = true;

    spdlog::debug("Processed block {} for fee estimation ({} tx)", height,
                  block.transactions.size() - 1);
}

std::optional<uint64_t> FeeEstimator::estimateFeeRate(uint32_t targetBlocks) const {
    if (history_.size() < MIN_SAMPLE_SIZE) {
        spdlog::debug("Insufficient data for fee estimation ({} samples)", history_.size());
        return std::nullopt;
    }

    // Get fee rates for transactions confirmed within target
    auto feeRates = getFeeRatesForTarget(targetBlocks);

    if (feeRates.empty()) {
        return std::nullopt;
    }

    // Sort fee rates
    std::sort(feeRates.begin(), feeRates.end());

    // Use median for robustness against outliers
    uint64_t medianRate = calculateMedian(feeRates);

    // Apply safety multiplier
    uint64_t estimatedRate =
        static_cast<uint64_t>(medianRate * CONFIRMATION_TARGET_MULTIPLIER);

    spdlog::debug("Estimated fee rate for {} blocks: {} sat/byte", targetBlocks, estimatedRate);

    return estimatedRate;
}

std::optional<uint64_t> FeeEstimator::estimateFee(const core::Transaction& tx,
                                                   uint32_t targetBlocks) const {
    auto feeRate = estimateFeeRate(targetBlocks);
    if (!feeRate) {
        return std::nullopt;
    }

    uint64_t txSize = tx.getSize();
    return *feeRate * txSize;
}

std::optional<FeeEstimator::FeeStats> FeeEstimator::getStats(uint32_t targetBlocks) const {
    if (history_.size() < MIN_SAMPLE_SIZE) {
        return std::nullopt;
    }

    // Update cache if dirty
    if (statsCacheDirty_) {
        updateStatsCache();
    }

    auto it = statsCache_.find(targetBlocks);
    if (it != statsCache_.end()) {
        return it->second;
    }

    // Calculate stats for this target
    auto stats = calculateStats(targetBlocks);
    statsCache_[targetBlocks] = stats;

    return stats;
}

void FeeEstimator::clear() {
    history_.clear();
    statsCache_.clear();
    statsCacheDirty_ = true;
}

void FeeEstimator::updateStatsCache() const {
    statsCache_.clear();

    // Pre-calculate stats for common targets
    std::vector<uint32_t> targets = {1, 3, 6, 12, 24, 48, 144};  // blocks

    for (uint32_t target : targets) {
        auto stats = calculateStats(target);
        statsCache_[target] = stats;
    }

    statsCacheDirty_ = false;
}

FeeEstimator::FeeStats FeeEstimator::calculateStats(uint32_t targetBlocks) const {
    auto feeRates = getFeeRatesForTarget(targetBlocks);

    FeeStats stats;
    stats.sampleSize = feeRates.size();

    if (feeRates.empty()) {
        stats.minFeeRate = 0;
        stats.maxFeeRate = 0;
        stats.medianFeeRate = 0;
        stats.avgFeeRate = 0;
        return stats;
    }

    std::sort(feeRates.begin(), feeRates.end());

    stats.minFeeRate = feeRates.front();
    stats.maxFeeRate = feeRates.back();
    stats.medianFeeRate = calculateMedian(feeRates);
    stats.avgFeeRate = calculateAverage(feeRates);

    return stats;
}

std::vector<uint64_t> FeeEstimator::getFeeRatesForTarget(uint32_t targetBlocks) const {
    std::vector<uint64_t> feeRates;
    feeRates.reserve(history_.size());

    for (const auto& record : history_) {
        // Include transactions that were confirmed within the target
        if (record.blocksWaited <= targetBlocks) {
            feeRates.push_back(record.feeRate);
        }
    }

    return feeRates;
}

uint64_t FeeEstimator::calculateMedian(const std::vector<uint64_t>& values) {
    if (values.empty()) {
        return 0;
    }

    size_t size = values.size();
    if (size % 2 == 0) {
        return (values[size / 2 - 1] + values[size / 2]) / 2;
    } else {
        return values[size / 2];
    }
}

uint64_t FeeEstimator::calculateAverage(const std::vector<uint64_t>& values) {
    if (values.empty()) {
        return 0;
    }

    uint64_t sum = std::accumulate(values.begin(), values.end(), 0ULL);
    return sum / values.size();
}

void FeeEstimator::pruneHistory() {
    // Keep only the most recent entries
    size_t toRemove = history_.size() - (MAX_HISTORY_SIZE * 3 / 4);  // Remove 25%

    if (toRemove > 0) {
        history_.erase(history_.begin(), history_.begin() + toRemove);
        spdlog::debug("Pruned {} fee history entries", toRemove);
    }
}

}  // namespace mempool
}  // namespace ubuntu
