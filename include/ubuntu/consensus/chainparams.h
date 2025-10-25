#pragma once

#include "ubuntu/crypto/hash.h"

#include <cstdint>
#include <string>

namespace ubuntu {
namespace consensus {

/**
 * @brief Network type (mainnet, testnet, regtest)
 */
enum class NetworkType {
    MAINNET,
    TESTNET,
    REGTEST  // Regression test network
};

/**
 * @brief Chain parameters defining consensus rules
 */
struct ChainParams {
    // Network identification
    NetworkType networkType;
    std::string networkName;
    uint32_t magicBytes;  // Network message magic bytes

    // Genesis block
    crypto::Hash256 genesisHash;
    uint32_t genesisTimestamp;
    uint32_t genesisNonce;
    uint32_t genesisDifficulty;

    // Proof of Work
    uint32_t powTargetTimespan;   // Time for difficulty adjustment (2016 blocks)
    uint32_t powTargetSpacing;    // Target time between blocks (60 seconds)
    uint32_t difficultyAdjustmentInterval;  // Blocks between adjustments (2016)
    bool allowMinDifficultyBlocks;  // For testnet
    bool noRetargeting;  // For regtest

    // Block validation
    uint64_t maxBlockSize;  // Maximum block size in bytes
    uint32_t coinbaseMaturity;  // Confirmations required for coinbase (100)
    uint64_t maxMoneySupply;  // Maximum coins that will ever exist

    // Mining rewards
    uint64_t initialBlockReward;  // 50 UBU
    uint32_t halvingInterval;  // Blocks between halvings (4 years worth)

    // Network
    uint16_t defaultPort;
    uint16_t rpcPort;

    // Address prefixes
    uint8_t base58PubkeyPrefix;  // For P2PKH addresses
    uint8_t base58ScriptPrefix;  // For P2SH addresses
    std::string bech32Prefix;    // For SegWit addresses

    // Checkpoints (block height -> hash)
    // Used to prevent reorganization beyond certain points
    std::vector<std::pair<uint32_t, crypto::Hash256>> checkpoints;
};

/**
 * @brief Get chain parameters for a specific network
 */
class ChainParamsFactory {
public:
    static ChainParams mainnet();
    static ChainParams testnet();
    static ChainParams regtest();

    static ChainParams forNetwork(NetworkType network);
};

/**
 * @brief Consensus constants
 */
namespace Constants {

// Coin denomination
constexpr uint64_t COIN = 100'000'000;  // 1 UBU = 10^8 satoshis
constexpr uint64_t CENT = 1'000'000;    // 0.01 UBU

// Supply limits
constexpr uint64_t MAX_MONEY = 21'000'000 * COIN;  // 21 million UBU max
constexpr uint64_t INITIAL_SUPPLY = 1'000'000'000'000 * COIN;  // 1 trillion UBU

// Block limits
constexpr uint64_t MAX_BLOCK_SIZE = 1'000'000;  // 1 MB
constexpr uint64_t MAX_BLOCK_WEIGHT = 4'000'000;  // 4 MB (for future SegWit)
constexpr uint32_t MAX_BLOCK_SIGOPS = 20'000;

// Transaction limits
constexpr uint64_t MAX_TX_SIZE = 100'000;  // 100 KB
constexpr uint64_t MIN_TX_FEE = 1000;  // 1000 satoshis minimum fee
constexpr uint64_t DUST_THRESHOLD = 546;  // Minimum output value

// Timelock
constexpr uint32_t LOCKTIME_THRESHOLD = 500'000'000;  // Distinguish height vs timestamp
constexpr uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

// Coinbase maturity
constexpr uint32_t COINBASE_MATURITY = 100;  // 100 confirmations

// PoW parameters
constexpr uint32_t POW_TARGET_TIMESPAN = 14 * 24 * 60 * 60;  // 2 weeks
constexpr uint32_t POW_TARGET_SPACING = 60;  // 1 minute
constexpr uint32_t DIFFICULTY_ADJUSTMENT_INTERVAL = POW_TARGET_TIMESPAN / POW_TARGET_SPACING;

// Maximum target (minimum difficulty)
constexpr uint32_t MAX_PROOF_OF_WORK = 0x1d00ffff;

}  // namespace Constants

}  // namespace consensus
}  // namespace ubuntu
