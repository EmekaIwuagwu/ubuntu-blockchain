#include "ubuntu/consensus/pow.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ubuntu {
namespace consensus {

// ============================================================================
// CompactTarget Implementation
// ============================================================================

crypto::Hash256 CompactTarget::toTarget() const {
    // Extract exponent and mantissa
    uint32_t exponent = (compact_ >> 24) & 0xFF;
    uint32_t mantissa = compact_ & 0x00FFFFFF;

    // Check for negative (invalid)
    if (mantissa & 0x00800000) {
        return crypto::Hash256::zero();
    }

    crypto::Hash256::DataType targetData = {};

    // Convert compact to full 256-bit target
    if (exponent <= 3) {
        mantissa >>= (8 * (3 - exponent));
        targetData[0] = mantissa & 0xFF;
        targetData[1] = (mantissa >> 8) & 0xFF;
        targetData[2] = (mantissa >> 16) & 0xFF;
    } else {
        size_t offset = exponent - 3;
        if (offset < 29) {  // Prevent overflow
            targetData[offset] = mantissa & 0xFF;
            targetData[offset + 1] = (mantissa >> 8) & 0xFF;
            targetData[offset + 2] = (mantissa >> 16) & 0xFF;
        }
    }

    return crypto::Hash256(targetData);
}

CompactTarget CompactTarget::fromTarget(const crypto::Hash256& target) {
    // Find the most significant byte
    int msb = 31;
    while (msb >= 0 && target.data()[msb] == 0) {
        --msb;
    }

    if (msb < 0) {
        return CompactTarget(0);
    }

    // Extract 3 bytes starting from MSB
    uint32_t mantissa = 0;
    int exponent = msb + 1;

    mantissa = target.data()[msb];
    if (msb >= 1) mantissa = (mantissa << 8) | target.data()[msb - 1];
    if (msb >= 2) mantissa = (mantissa << 8) | target.data()[msb - 2];

    // Normalize mantissa to 3 bytes
    if (mantissa & 0xFF000000) {
        mantissa >>= 8;
        exponent++;
    }

    uint32_t compact = (exponent << 24) | (mantissa & 0x00FFFFFF);
    return CompactTarget(compact);
}

CompactTarget CompactTarget::max() {
    // Maximum target (minimum difficulty)
    // Bitcoin genesis block: 0x1d00ffff
    return CompactTarget(0x1d00ffff);
}

// ============================================================================
// PoW Functions
// ============================================================================

namespace PoW {

bool checkProofOfWork(const crypto::Hash256& blockHash,
                      const crypto::Hash256& target) {
    // Block hash must be <= target
    // Reverse byte order for comparison (little-endian vs big-endian)
    return blockHash <= target;
}

bool checkProofOfWork(const crypto::Hash256& blockHash,
                      uint32_t compactTarget) {
    CompactTarget compact(compactTarget);
    auto target = compact.toTarget();
    return checkProofOfWork(blockHash, target);
}

bool verifyBlockPoW(const core::Block& block) {
    auto blockHash = block.calculateHash();
    return checkProofOfWork(blockHash, block.header.difficulty);
}

double getDifficulty(uint32_t compactTarget) {
    // Difficulty = max_target / current_target
    CompactTarget maxTarget = CompactTarget::max();
    CompactTarget currentTarget(compactTarget);

    auto maxTargetHash = maxTarget.toTarget();
    auto currentTargetHash = currentTarget.toTarget();

    // Simplified difficulty calculation
    // Full implementation would do precise 256-bit division
    double maxValue = 0.0;
    double currentValue = 0.0;

    for (int i = 31; i >= 0; --i) {
        maxValue = maxValue * 256.0 + maxTargetHash.data()[i];
        currentValue = currentValue * 256.0 + currentTargetHash.data()[i];
    }

    if (currentValue <= 0.0) {
        return 0.0;
    }

    return maxValue / currentValue;
}

uint32_t calculateNextDifficulty(uint32_t previousTarget,
                                 uint32_t actualTimespan,
                                 uint32_t targetTimespan) {
    // Clamp actual timespan to prevent extreme changes (4x up or down)
    uint32_t minTimespan = targetTimespan / 4;
    uint32_t maxTimespan = targetTimespan * 4;
    actualTimespan = std::clamp(actualTimespan, minTimespan, maxTimespan);

    // Calculate new target
    // new_target = old_target * (actual_time / expected_time)
    CompactTarget oldTarget(previousTarget);
    auto oldTargetHash = oldTarget.toTarget();

    // Simplified: multiply by ratio (actualTimespan / targetTimespan)
    // Full implementation would do proper 256-bit arithmetic

    // For now, adjust the compact representation
    uint64_t newCompact = static_cast<uint64_t>(previousTarget) *
                          actualTimespan / targetTimespan;

    // Ensure we don't exceed max target
    if (newCompact > 0x1d00ffff) {
        newCompact = 0x1d00ffff;
    }

    return static_cast<uint32_t>(newCompact);
}

uint64_t getBlockWork(uint32_t compactTarget) {
    CompactTarget target(compactTarget);
    auto targetHash = target.toTarget();

    // Work = 2^256 / (target + 1)
    // Simplified calculation using the first 8 bytes
    uint64_t targetValue = 0;
    for (int i = 0; i < 8; ++i) {
        targetValue = (targetValue << 8) | targetHash.data()[31 - i];
    }

    if (targetValue == 0) {
        return 0;
    }

    // Approximate work
    return 0xFFFFFFFFFFFFFFFFULL / targetValue;
}

uint64_t calculateMedianTimePast(const std::vector<uint64_t>& timestamps) {
    if (timestamps.empty()) {
        return 0;
    }

    // Create mutable copy for sorting
    std::vector<uint64_t> sortedTimestamps = timestamps;
    std::sort(sortedTimestamps.begin(), sortedTimestamps.end());

    // Return median value
    size_t medianIndex = sortedTimestamps.size() / 2;

    if (sortedTimestamps.size() % 2 == 0 && sortedTimestamps.size() > 1) {
        // Even number of elements: average of middle two
        return (sortedTimestamps[medianIndex - 1] + sortedTimestamps[medianIndex]) / 2;
    } else {
        // Odd number of elements: middle element
        return sortedTimestamps[medianIndex];
    }
}

bool validateTimestamp(uint64_t blockTimestamp,
                       const std::vector<uint64_t>& previousTimestamps,
                       uint64_t currentTime) {
    // 1. Check Median-Time-Past (MTP) rule
    // Block timestamp must be greater than MTP of last 11 blocks
    if (!previousTimestamps.empty()) {
        uint64_t medianTimePast = calculateMedianTimePast(previousTimestamps);

        if (blockTimestamp <= medianTimePast) {
            // Timestamp too old - fails MTP check
            return false;
        }
    }

    // 2. Check future time limit
    // Block timestamp must not be more than 2 hours in the future
    if (!checkFutureTimeLimit(blockTimestamp, currentTime)) {
        return false;
    }

    return true;
}

bool checkFutureTimeLimit(uint64_t blockTimestamp, uint64_t currentTime) {
    // Get current time if not provided
    if (currentTime == 0) {
        currentTime = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    }

    // Maximum allowed time drift: 2 hours (7200 seconds)
    const uint64_t MAX_FUTURE_BLOCK_TIME = 7200;

    // Block timestamp must not be more than 2 hours in the future
    if (blockTimestamp > currentTime + MAX_FUTURE_BLOCK_TIME) {
        return false;
    }

    return true;
}

}  // namespace PoW

// ============================================================================
// Miner Implementation
// ============================================================================

bool Miner::mineBlock(core::Block& block,
                      std::function<bool()> shouldStop) {
    CompactTarget target(block.header.difficulty);
    auto targetHash = target.toTarget();

    uint64_t hashCount = 0;
    const uint64_t updateInterval = 1'000'000;  // Update every 1M hashes

    while (!shouldStop()) {
        // Calculate block hash
        auto blockHash = block.calculateHash();

        // Check if it meets difficulty
        if (PoW::checkProofOfWork(blockHash, targetHash)) {
            return true;  // Found valid nonce!
        }

        // Increment nonce and try again
        block.header.nonce++;
        hashCount++;

        // Periodically update timestamp to avoid stale work
        if (hashCount % updateInterval == 0) {
            // In production, would update timestamp here
            // block.header.timestamp = getCurrentTimestamp();
        }

        // Check for nonce overflow (very unlikely with 32-bit nonce)
        if (block.header.nonce == 0) {
            // Wrapped around - would need to modify timestamp or other field
            block.header.timestamp++;
        }
    }

    return false;  // Stopped before finding valid nonce
}

bool Miner::mineBlockWithLimit(core::Block& block, uint64_t maxIterations) {
    uint64_t iterations = 0;

    auto shouldStop = [&iterations, maxIterations]() {
        return iterations++ >= maxIterations;
    };

    return mineBlock(block, shouldStop);
}

double Miner::estimateHashRate(uint64_t hashesComputed, double timeSeconds) {
    if (timeSeconds <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(hashesComputed) / timeSeconds;
}

}  // namespace consensus
}  // namespace ubuntu
