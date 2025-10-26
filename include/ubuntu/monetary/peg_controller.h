#pragma once

#include "ubuntu/monetary/peg_state.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#include "ubuntu/storage/database.h"

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace ubuntu {
namespace monetary {

/**
 * @brief Algorithmic peg controller for maintaining 1 UBU = 1 USD target
 *
 * This controller implements a proportional (or PID) control system that adjusts
 * the money supply each epoch to drive the market price toward $1.00 USD.
 *
 * Key features:
 * - Deterministic integer-only arithmetic (no floating-point)
 * - Configurable dead-band to prevent excessive adjustments
 * - Per-epoch expansion/contraction caps for safety
 * - Circuit breaker for emergency situations
 * - Bond issuance for debt-based contraction
 * - Full state persistence to RocksDB
 * - Comprehensive audit trail
 *
 * Thread safety: All public methods are thread-safe via mutex.
 *
 * @example
 * auto ledger = std::make_shared<ledger::LedgerAdapter>(blockchain);
 * auto oracle = OracleFactory::create("stub", "");
 * auto db = std::make_shared<storage::Database>("./data");
 *
 * PegConfig config;
 * config.enabled = true;
 * config.k_ppm = 50'000;  // 5% proportional gain
 *
 * PegController controller(ledger, oracle, db, config);
 *
 * // Each epoch (called from scheduler):
 * controller.run_epoch(epoch_id, block_height, timestamp);
 */
class PegController {
public:
    /**
     * @brief Construct a new Peg Controller
     *
     * @param ledger Interface for querying supply and executing mint/burn
     * @param oracle Price oracle for fetching USD exchange rate
     * @param db RocksDB instance for state persistence
     * @param config Initial configuration parameters
     *
     * @throws std::runtime_error if ledger, oracle, or db is nullptr
     */
    PegController(
        std::shared_ptr<ledger::LedgerAdapter> ledger,
        std::shared_ptr<IOracle> oracle,
        std::shared_ptr<storage::Database> db,
        const PegConfig& config);

    /**
     * @brief Destroy the Peg Controller and persist final state
     */
    ~PegController();

    /**
     * @brief Execute one epoch of the peg mechanism
     *
     * This is the main entry point called by the scheduler each epoch.
     *
     * Algorithm:
     * 1. Fetch current price from oracle (validate staleness)
     * 2. Check circuit breaker conditions
     * 3. Calculate price error: error = price - 1.00
     * 4. Check dead-band: if |error| < deadband, do nothing
     * 5. Calculate supply delta: ΔSupply = k × error × supply
     * 6. Apply per-epoch caps: ΔSupply = clamp(ΔSupply, -5%, +5%)
     * 7. Execute expansion (mint) or contraction (burn/bonds)
     * 8. Update PID state (if enabled)
     * 9. Persist state and event to RocksDB
     *
     * @param epoch_id Monotonically increasing epoch counter
     * @param block_height Current blockchain height
     * @param timestamp Current Unix timestamp (seconds)
     *
     * @return true if epoch executed successfully, false on error
     *
     * @note If peg is disabled (config.enabled = false), returns true immediately
     * @note If circuit breaker is active, no action is taken
     */
    bool run_epoch(uint64_t epoch_id, uint64_t block_height, uint64_t timestamp);

    /**
     * @brief Get current peg state (read-only)
     *
     * @return Copy of current PegState
     */
    PegState get_state() const;

    /**
     * @brief Update configuration parameters
     *
     * @param new_config New configuration to apply
     *
     * @note Changes take effect on the next epoch
     * @note Cannot enable peg if circuit breaker is active
     */
    void update_config(const PegConfig& new_config);

    /**
     * @brief Retrieve recent epoch events from audit trail
     *
     * @param count Number of recent events to fetch (default 100)
     * @return Vector of PegEvent ordered newest to oldest
     */
    std::vector<PegEvent> get_recent_events(size_t count = 100) const;

    /**
     * @brief Trigger emergency stop of peg mechanism
     *
     * Sets circuit_breaker_triggered = true and disables peg.
     *
     * @param reason Human-readable reason for emergency stop
     *
     * @note Once triggered, requires manual intervention to reset
     */
    void emergency_stop(const std::string& reason);

    /**
     * @brief Check if peg controller is healthy and operational
     *
     * @return true if:
     *   - Peg is enabled
     *   - Circuit breaker not triggered
     *   - Oracle price is fresh (< max_age)
     *   - Ledger adapter is functional
     */
    bool is_healthy() const;

    /**
     * @brief Get current configuration
     *
     * @return Copy of current PegConfig
     */
    PegConfig get_config() const;

    /**
     * @brief Reset circuit breaker (requires manual intervention)
     *
     * @param reason Reason for reset (logged for audit)
     *
     * @note Only call this after resolving the underlying issue
     */
    void reset_circuit_breaker(const std::string& reason);

private:
    /**
     * @brief Calculate supply delta using proportional control
     *
     * Formula: ΔSupply = k × (price - target) × supply
     *
     * All math is done in scaled integers:
     * - price_scaled: price × PRICE_SCALE (e.g., 1.05 USD = 1,050,000)
     * - supply: total supply in smallest units (satoshi-like)
     * - k: proportional gain in PPM (e.g., 0.05 = 50,000 ppm)
     *
     * @param price_scaled Current market price (scaled)
     * @param supply Current circulating supply
     *
     * @return Calculated delta (positive = expand, negative = contract)
     */
    int128_t calculate_delta_proportional(int64_t price_scaled, int128_t supply) const;

    /**
     * @brief Calculate supply delta using full PID control
     *
     * Formula: ΔSupply = (kp × error + ki × integral + kd × derivative) × supply
     *
     * Updates state_.integral and state_.prev_error_scaled
     *
     * @param price_scaled Current market price (scaled)
     * @param supply Current circulating supply
     *
     * @return Calculated delta (positive = expand, negative = contract)
     */
    int128_t calculate_delta_pid(int64_t price_scaled, int128_t supply);

    /**
     * @brief Apply per-epoch expansion/contraction caps
     *
     * Ensures |delta| ≤ max_X_ppm × supply / PPM_SCALE
     *
     * @param delta Raw calculated delta
     * @param supply Current supply
     *
     * @return Clamped delta
     */
    int128_t apply_caps(int128_t delta, int128_t supply) const;

    /**
     * @brief Execute supply expansion (minting)
     *
     * Calls ledger_->mint_to_treasury(amount, treasury_address)
     *
     * @param amount Amount to mint (in smallest units)
     * @param epoch_id Current epoch ID
     *
     * @return true if successful, false on error
     */
    bool execute_expansion(int128_t amount, uint64_t epoch_id);

    /**
     * @brief Execute supply contraction (burning or bond issuance)
     *
     * Strategy:
     * 1. Try to burn from treasury first
     * 2. If treasury insufficient, issue bonds for remaining amount
     *
     * @param amount Amount to contract (positive value)
     * @param epoch_id Current epoch ID
     *
     * @return true if successful, false on error
     */
    bool execute_contraction(int128_t amount, uint64_t epoch_id);

    /**
     * @brief Issue bonds for debt-based contraction
     *
     * Creates BondState entry and persists to database.
     *
     * @param amount Amount of bonds to issue
     * @param epoch_id Current epoch ID
     *
     * @return true if successful, false on error
     */
    bool issue_bonds(int128_t amount, uint64_t epoch_id);

    /**
     * @brief Persist current state to RocksDB
     *
     * Key: "peg_state:current"
     * Value: state_.serialize()
     *
     * @return true if successful, false on error
     */
    bool persist_state();

    /**
     * @brief Persist epoch event to audit trail
     *
     * Key: "peg_events:<epoch_id>"
     * Value: event.serialize()
     *
     * @param event Event to persist
     *
     * @return true if successful, false on error
     */
    bool persist_event(const PegEvent& event);

    /**
     * @brief Load state from RocksDB (called in constructor)
     *
     * @return true if state loaded, false if not found (starts fresh)
     */
    bool load_state();

    /**
     * @brief Check if circuit breaker should trigger
     *
     * Triggers if |price - 1.00| > circuit_breaker_ppm
     *
     * @param price_scaled Current price (scaled)
     *
     * @return true if should trigger
     */
    bool should_trigger_circuit_breaker(int64_t price_scaled) const;

    // Dependencies
    std::shared_ptr<ledger::LedgerAdapter> ledger_;
    std::shared_ptr<IOracle> oracle_;
    std::shared_ptr<storage::Database> db_;

    // Configuration and state
    PegConfig config_;
    PegState state_;

    // Thread safety
    mutable std::mutex mutex_;
};

}  // namespace monetary
}  // namespace ubuntu
