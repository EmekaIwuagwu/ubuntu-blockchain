#pragma once

#include "ubuntu/core/block.h"
#include "ubuntu/crypto/hash.h"

#include <cstdint>

namespace ubuntu {
namespace consensus {

/**
 * @brief Compact representation of difficulty target
 *
 * Uses the same compact format as Bitcoin (nBits).
 * Format: 0xMMNNNNNN where MM is the exponent and NNNNNN is the mantissa.
 */
class CompactTarget {
public:
    explicit CompactTarget(uint32_t compact) : compact_(compact) {}

    // Convert compact to full 256-bit target
    crypto::Hash256 toTarget() const;

    // Convert full target to compact
    static CompactTarget fromTarget(const crypto::Hash256& target);

    // Get compact representation
    uint32_t getCompact() const { return compact_; }

    // Maximum difficulty target (minimum difficulty)
    static CompactTarget max();

private:
    uint32_t compact_;
};

/**
 * @brief Proof of Work utilities
 */
namespace PoW {

/**
 * @brief Check if a block hash meets the difficulty target
 *
 * @param blockHash The block's hash
 * @param target The difficulty target
 * @return true if hash <= target (block is valid)
 */
bool checkProofOfWork(const crypto::Hash256& blockHash,
                      const crypto::Hash256& target);

/**
 * @brief Check if a block hash meets the compact difficulty
 *
 * @param blockHash The block's hash
 * @param compactTarget Compact difficulty representation
 * @return true if hash meets difficulty
 */
bool checkProofOfWork(const crypto::Hash256& blockHash,
                      uint32_t compactTarget);

/**
 * @brief Verify a block's proof of work
 *
 * @param block The block to verify
 * @return true if PoW is valid
 */
bool verifyBlockPoW(const core::Block& block);

/**
 * @brief Calculate difficulty from compact target
 *
 * Returns the difficulty as a floating-point number relative
 * to the genesis block difficulty.
 *
 * @param compactTarget Compact difficulty representation
 * @return Difficulty value
 */
double getDifficulty(uint32_t compactTarget);

/**
 * @brief Calculate the next difficulty target
 *
 * Implements Bitcoin-style difficulty adjustment every 2016 blocks.
 *
 * @param previousTarget Previous difficulty target
 * @param actualTimespan Actual time to mine last 2016 blocks (seconds)
 * @param targetTimespan Expected time (2016 * 60 seconds = ~33.6 hours)
 * @return New difficulty target
 */
uint32_t calculateNextDifficulty(uint32_t previousTarget,
                                 uint32_t actualTimespan,
                                 uint32_t targetTimespan);

/**
 * @brief Calculate total work (cumulative difficulty)
 *
 * Used for chain selection - chain with most work wins.
 *
 * @param compactTarget Compact difficulty representation
 * @return Work value (approximation of 2^256 / target)
 */
uint64_t getBlockWork(uint32_t compactTarget);

/**
 * @brief Calculate Median-Time-Past (MTP) from previous blocks
 *
 * Implements BIP-113: Median time past as endpoint for locktime calculations.
 * Prevents miners from manipulating timestamps.
 *
 * @param timestamps Vector of previous block timestamps (last 11 blocks)
 * @return Median timestamp
 */
uint64_t calculateMedianTimePast(const std::vector<uint64_t>& timestamps);

/**
 * @brief Validate block timestamp
 *
 * Checks:
 * 1. Timestamp must be greater than Median-Time-Past of last 11 blocks
 * 2. Timestamp must not be more than 2 hours in the future
 *
 * @param blockTimestamp Block timestamp to validate
 * @param previousTimestamps Timestamps of previous 11 blocks (for MTP)
 * @param currentTime Current network time (optional, uses system time if 0)
 * @return true if timestamp is valid
 */
bool validateTimestamp(uint64_t blockTimestamp,
                       const std::vector<uint64_t>& previousTimestamps,
                       uint64_t currentTime = 0);

/**
 * @brief Check if timestamp is in acceptable future range
 *
 * Blocks with timestamps more than 2 hours in the future are rejected.
 *
 * @param blockTimestamp Block timestamp
 * @param currentTime Current network time (optional, uses system time if 0)
 * @return true if timestamp is not too far in future
 */
bool checkFutureTimeLimit(uint64_t blockTimestamp, uint64_t currentTime = 0);

}  // namespace PoW

/**
 * @brief Miner for proof of work
 */
class Miner {
public:
    /**
     * @brief Mine a block by finding a valid nonce
     *
     * Increments the nonce until the block hash meets the difficulty target.
     * Returns early if shouldStop returns true.
     *
     * @param block Block to mine (will modify nonce)
     * @param shouldStop Callback to check if mining should stop
     * @return true if valid nonce found, false if stopped
     */
    static bool mineBlock(core::Block& block,
                          std::function<bool()> shouldStop = []() { return false; });

    /**
     * @brief Mine with maximum iterations limit
     *
     * @param block Block to mine
     * @param maxIterations Maximum nonce attempts
     * @return true if valid nonce found within iterations
     */
    static bool mineBlockWithLimit(core::Block& block, uint64_t maxIterations);

    /**
     * @brief Estimate hash rate from mining statistics
     *
     * @param hashesComputed Number of hashes computed
     * @param timeSeconds Time taken in seconds
     * @return Estimated hash rate (hashes per second)
     */
    static double estimateHashRate(uint64_t hashesComputed, double timeSeconds);
};

}  // namespace consensus
}  // namespace ubuntu
