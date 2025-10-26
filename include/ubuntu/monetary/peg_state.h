#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ubuntu {
namespace monetary {

using int128_t = boost::multiprecision::int128_t;

/**
 * @brief Mathematical constants for deterministic fixed-point arithmetic
 * 
 * All calculations use integer arithmetic with scaling factors to avoid
 * floating-point non-determinism.
 */
struct PegConstants {
    // Price scaling: 1.000000 USD = 1,000,000
    static constexpr int64_t PRICE_SCALE = 1'000'000;
    
    // Coin scaling: 1 UBU = 100,000,000 units (like Bitcoin satoshis)
    static constexpr int64_t COIN_SCALE = 100'000'000;
    
    // Scaling for proportional gain (k) and percentages
    static constexpr int64_t K_SCALE = 1'000'000;
    static constexpr int64_t PPM_SCALE = 1'000'000; // Parts per million
    
    // Target price (1 USD in scaled units)
    static constexpr int64_t TARGET_PRICE = PRICE_SCALE;
};

/**
 * @brief Configuration parameters for the peg controller
 * 
 * All parameters use scaled integer representation for determinism.
 */
struct PegConfig {
    bool enabled{false};
    
    // Epoch configuration
    uint64_t epoch_seconds{3600};        // 1 hour default
    uint64_t epoch_blocks{600};          // Alternative: blocks-based epochs
    bool use_block_epochs{false};        // If true, use blocks; else use time
    
    // Controller parameters (all scaled by PPM_SCALE = 1,000,000)
    int64_t deadband_ppm{10'000};        // 1% dead-band (10,000 ppm)
    int64_t k_ppm{50'000};               // k = 0.05 (50,000 ppm)
    int64_t max_expansion_ppm{50'000};   // 5% max expansion per epoch
    int64_t max_contraction_ppm{50'000}; // 5% max contraction per epoch
    
    // PID controller (optional, set ki/kd > 0 to enable)
    int64_t ki_ppm{0};                   // Integral gain
    int64_t kd_ppm{0};                   // Derivative gain
    
    // Safety parameters
    uint64_t oracle_max_age_seconds{600}; // 10 minutes
    int64_t circuit_breaker_ppm{500'000}; // 50% emergency threshold
    int128_t max_bond_debt{0};           // Maximum total bond debt (0 = unlimited)
    
    // Treasury configuration
    std::string treasury_address;        // Protocol-owned address for expansion
    
    // Scaling constants (read-only, derived from PegConstants)
    int64_t price_scale{PegConstants::PRICE_SCALE};
    int64_t coin_scale{PegConstants::COIN_SCALE};
};

/**
 * @brief Current state of the peg controller
 * 
 * Persisted to RocksDB after each epoch for auditability and recovery.
 */
struct PegState {
    uint64_t epoch_id{0};
    uint64_t timestamp{0};
    uint64_t block_height{0};
    
    // Latest oracle price (scaled by PRICE_SCALE)
    int64_t last_price_scaled{PegConstants::TARGET_PRICE};
    
    // Supply tracking (in smallest units)
    int128_t last_supply{0};
    int128_t last_delta{0};              // Positive = expansion, negative = contraction
    
    // Bond tracking for contractions
    int128_t total_bond_debt{0};
    int128_t bonds_issued_this_epoch{0};
    int128_t bonds_redeemed_this_epoch{0};
    
    // PID controller state (if enabled)
    int128_t integral{0};                // Accumulated error integral
    int64_t prev_error_scaled{0};        // Previous error for derivative
    
    // Diagnostics
    std::string last_action;             // "expand", "contract", "none", "deadband"
    std::string last_reason;             // Detailed reason for action/inaction
    bool circuit_breaker_triggered{false};
    
    /**
     * @brief Serialize state to bytes for RocksDB persistence
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize state from bytes
     */
    static PegState deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief Event log entry for each epoch
 * 
 * Persisted separately for audit trail and querying.
 */
struct PegEvent {
    uint64_t epoch_id;
    uint64_t timestamp;
    uint64_t block_height;
    int64_t price_scaled;
    int128_t supply;
    int128_t delta;
    std::string action;
    std::string reason;
    
    std::vector<uint8_t> serialize() const;
    static PegEvent deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief Bond state for tracking debt-based contraction
 * 
 * When supply needs to contract but treasury is insufficient,
 * bonds are issued as future claims on expansion.
 */
struct BondState {
    uint64_t bond_id;
    int128_t amount;                     // Amount of bonds
    uint64_t issued_epoch;
    uint64_t maturity_epoch;            // Future epoch for redemption
    int64_t discount_rate_ppm;          // Discount rate (e.g., 5% = 50,000 ppm)
    
    std::vector<uint8_t> serialize() const;
    static BondState deserialize(const std::vector<uint8_t>& data);
};

} // namespace monetary
} // namespace ubuntu
