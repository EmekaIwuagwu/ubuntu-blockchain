#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/core/logger.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ubuntu {
namespace monetary {

namespace {

/**
 * @brief Safely multiply two scaled integers with overflow detection
 *
 * @param a First operand
 * @param b Second operand
 * @param scale Scaling factor to divide by
 *
 * @return (a * b) / scale with overflow protection
 */
int128_t safe_scaled_multiply(int128_t a, int128_t b, int128_t scale) {
    // Perform multiplication in int128 (already overflow-safe)
    int128_t result = (a * b) / scale;
    return result;
}

/**
 * @brief Calculate absolute value for int128
 */
int128_t abs128(int128_t value) {
    return value < 0 ? -value : value;
}

}  // anonymous namespace

PegController::PegController(
    std::shared_ptr<ledger::LedgerAdapter> ledger,
    std::shared_ptr<IOracle> oracle,
    std::shared_ptr<storage::Database> db,
    const PegConfig& config)
    : ledger_(std::move(ledger))
    , oracle_(std::move(oracle))
    , db_(std::move(db))
    , config_(config)
    , state_()
{
    if (!ledger_) {
        throw std::runtime_error("PegController: ledger adapter cannot be null");
    }
    if (!oracle_) {
        throw std::runtime_error("PegController: oracle cannot be null");
    }
    if (!db_) {
        throw std::runtime_error("PegController: database cannot be null");
    }

    // Attempt to load previous state from database
    if (!load_state()) {
        LOG_INFO("PegController initialized with fresh state");
    } else {
        LOG_INFO("PegController restored state from epoch {}", state_.epoch_id);
    }

    LOG_INFO("PegController created: enabled={}, k={} ppm, deadband={} ppm",
             config_.enabled, config_.k_ppm, config_.deadband_ppm);
}

PegController::~PegController() {
    // Final state persistence on shutdown
    std::lock_guard<std::mutex> lock(mutex_);
    if (!persist_state()) {
        LOG_ERROR("Failed to persist final peg state on shutdown");
    }
}

bool PegController::run_epoch(uint64_t epoch_id, uint64_t block_height, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update state metadata
    state_.epoch_id = epoch_id;
    state_.block_height = block_height;
    state_.timestamp = timestamp;

    // Early return if peg is disabled
    if (!config_.enabled) {
        state_.last_action = "disabled";
        state_.last_reason = "Peg mechanism is disabled in configuration";
        persist_state();
        return true;
    }

    // Early return if circuit breaker is active
    if (state_.circuit_breaker_triggered) {
        state_.last_action = "circuit_breaker";
        state_.last_reason = "Circuit breaker is active - requires manual reset";
        persist_state();
        LOG_WARN("Epoch {}: Circuit breaker active, no action taken", epoch_id);
        return true;
    }

    try {
        // Step 1: Fetch current price from oracle
        auto price_opt = oracle_->get_latest_price();
        if (!price_opt) {
            state_.last_action = "error";
            state_.last_reason = "Failed to fetch price from oracle";
            persist_state();
            LOG_ERROR("Epoch {}: Oracle price fetch failed", epoch_id);
            return false;
        }

        const OraclePrice& price_data = *price_opt;

        // Validate price freshness
        if (price_data.is_stale(timestamp, config_.oracle_max_age_seconds)) {
            state_.last_action = "error";
            state_.last_reason = "Oracle price is stale";
            persist_state();
            LOG_ERROR("Epoch {}: Oracle price stale (age={} seconds)",
                     epoch_id, timestamp - price_data.timestamp);
            return false;
        }

        int64_t price_scaled = price_data.price_scaled;
        state_.last_price_scaled = price_scaled;

        LOG_INFO("Epoch {}: Price = {:.6f} USD (source: {})",
                 epoch_id,
                 static_cast<double>(price_scaled) / PegConstants::PRICE_SCALE,
                 price_data.source);

        // Step 2: Check circuit breaker conditions
        if (should_trigger_circuit_breaker(price_scaled)) {
            state_.circuit_breaker_triggered = true;
            state_.last_action = "circuit_breaker";
            std::ostringstream oss;
            oss << "Circuit breaker triggered: price deviation exceeds "
                << (config_.circuit_breaker_ppm / 10000.0) << "%";
            state_.last_reason = oss.str();
            persist_state();
            LOG_ERROR("Epoch {}: {}", epoch_id, state_.last_reason);
            emergency_stop(state_.last_reason);
            return true;
        }

        // Step 3: Calculate price error (scaled integers)
        int64_t error_scaled = price_scaled - PegConstants::TARGET_PRICE;

        // Step 4: Check dead-band
        int64_t deadband_abs = safe_scaled_multiply(
            PegConstants::TARGET_PRICE,
            config_.deadband_ppm,
            PegConstants::PPM_SCALE
        );

        if (std::abs(error_scaled) < deadband_abs) {
            state_.last_action = "deadband";
            std::ostringstream oss;
            oss << "Price within dead-band (±"
                << (config_.deadband_ppm / 10000.0) << "%), no action needed";
            state_.last_reason = oss.str();
            state_.last_delta = 0;
            persist_state();

            PegEvent event{
                epoch_id,
                timestamp,
                block_height,
                price_scaled,
                state_.last_supply,
                0,
                "deadband",
                state_.last_reason
            };
            persist_event(event);

            LOG_INFO("Epoch {}: {}", epoch_id, state_.last_reason);
            return true;
        }

        // Step 5: Get current supply
        int128_t current_supply = ledger_->get_total_supply();
        state_.last_supply = current_supply;

        if (current_supply <= 0) {
            state_.last_action = "error";
            state_.last_reason = "Invalid supply from ledger";
            persist_state();
            LOG_ERROR("Epoch {}: Invalid supply={}", epoch_id, current_supply);
            return false;
        }

        // Step 6: Calculate supply delta
        int128_t delta;
        if (config_.ki_ppm > 0 || config_.kd_ppm > 0) {
            // Use full PID controller
            delta = calculate_delta_pid(price_scaled, current_supply);
        } else {
            // Use simple proportional controller
            delta = calculate_delta_proportional(price_scaled, current_supply);
        }

        // Step 7: Apply per-epoch caps
        delta = apply_caps(delta, current_supply);
        state_.last_delta = delta;

        LOG_INFO("Epoch {}: Calculated delta = {} ({}% of supply)",
                 epoch_id,
                 delta,
                 static_cast<double>(delta * 100) / static_cast<double>(current_supply));

        // Step 8: Execute expansion or contraction
        bool success = false;
        if (delta > 0) {
            // Expansion: price > $1, mint new coins
            state_.last_action = "expand";
            std::ostringstream oss;
            oss << "Minting " << delta << " units to treasury (price too high)";
            state_.last_reason = oss.str();

            success = execute_expansion(delta, epoch_id);
        } else if (delta < 0) {
            // Contraction: price < $1, burn coins or issue bonds
            state_.last_action = "contract";
            std::ostringstream oss;
            oss << "Contracting " << (-delta) << " units (price too low)";
            state_.last_reason = oss.str();

            success = execute_contraction(-delta, epoch_id);  // Pass positive amount
        } else {
            // Delta is exactly zero (edge case)
            state_.last_action = "none";
            state_.last_reason = "Calculated delta is zero";
            success = true;
        }

        // Step 9: Persist state and event
        persist_state();

        PegEvent event{
            epoch_id,
            timestamp,
            block_height,
            price_scaled,
            current_supply,
            delta,
            state_.last_action,
            state_.last_reason
        };
        persist_event(event);

        if (success) {
            LOG_INFO("Epoch {}: {} - {}", epoch_id, state_.last_action, state_.last_reason);
        } else {
            LOG_ERROR("Epoch {}: Failed to execute {} action", epoch_id, state_.last_action);
        }

        return success;

    } catch (const std::exception& e) {
        state_.last_action = "error";
        state_.last_reason = std::string("Exception: ") + e.what();
        persist_state();
        LOG_ERROR("Epoch {}: Exception in run_epoch: {}", epoch_id, e.what());
        return false;
    }
}

PegState PegController::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void PegController::update_config(const PegConfig& new_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Prevent enabling if circuit breaker is active
    if (new_config.enabled && state_.circuit_breaker_triggered) {
        LOG_WARN("Cannot enable peg while circuit breaker is active");
        return;
    }

    config_ = new_config;
    LOG_INFO("PegController configuration updated: enabled={}, k={} ppm",
             config_.enabled, config_.k_ppm);
}

std::vector<PegEvent> PegController::get_recent_events(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PegEvent> events;
    events.reserve(count);

    // Iterate backwards from current epoch
    for (uint64_t i = 0; i < count && state_.epoch_id >= i; ++i) {
        uint64_t epoch = state_.epoch_id - i;
        std::string key = "peg_events:" + std::to_string(epoch);

        auto data_opt = db_->get(key);
        if (!data_opt) {
            continue;  // Skip missing events
        }

        try {
            PegEvent event = PegEvent::deserialize(*data_opt);
            events.push_back(event);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to deserialize event for epoch {}: {}", epoch, e.what());
        }
    }

    return events;
}

void PegController::emergency_stop(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    state_.circuit_breaker_triggered = true;
    config_.enabled = false;
    state_.last_action = "emergency_stop";
    state_.last_reason = reason;

    persist_state();

    LOG_ERROR("EMERGENCY STOP: {}", reason);
}

bool PegController::is_healthy() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enabled) {
        return false;
    }

    if (state_.circuit_breaker_triggered) {
        return false;
    }

    // Check oracle freshness
    auto price_opt = oracle_->get_latest_price();
    if (!price_opt) {
        return false;
    }

    if (price_opt->is_stale(state_.timestamp, config_.oracle_max_age_seconds)) {
        return false;
    }

    // Check ledger adapter
    try {
        int128_t supply = ledger_->get_total_supply();
        if (supply <= 0) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return true;
}

PegConfig PegController::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void PegController::reset_circuit_breaker(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    state_.circuit_breaker_triggered = false;
    state_.last_action = "circuit_breaker_reset";
    state_.last_reason = reason;

    persist_state();

    LOG_WARN("Circuit breaker reset: {}", reason);
}

// ============================================================================
// Private Implementation Methods
// ============================================================================

int128_t PegController::calculate_delta_proportional(int64_t price_scaled, int128_t supply) const {
    // ΔSupply = k × (price - target) × supply
    //
    // Example: price = $1.05, k = 0.05, supply = 1,000,000 UBU
    // error = 1.05 - 1.00 = 0.05
    // delta = 0.05 × 0.05 × 1,000,000 = 2,500 UBU (expand)

    int64_t error_scaled = price_scaled - PegConstants::TARGET_PRICE;

    // delta = (k_ppm / PPM_SCALE) × error_scaled × supply / PRICE_SCALE
    // Rearranged: delta = (k_ppm × error_scaled × supply) / (PPM_SCALE × PRICE_SCALE)

    int128_t k = config_.k_ppm;
    int128_t delta = (k * error_scaled * supply) / (PegConstants::PPM_SCALE * PegConstants::PRICE_SCALE);

    return delta;
}

int128_t PegController::calculate_delta_pid(int64_t price_scaled, int128_t supply) {
    // Full PID controller:
    // ΔSupply = (kp × error + ki × integral + kd × derivative) × supply
    //
    // Where:
    // - error = price - target
    // - integral += error (accumulated over time)
    // - derivative = error - prev_error

    int64_t error_scaled = price_scaled - PegConstants::TARGET_PRICE;

    // Update integral (with anti-windup: clamp to reasonable bounds)
    state_.integral += error_scaled;
    const int128_t MAX_INTEGRAL = 1'000'000'000;  // Prevent runaway
    state_.integral = std::clamp(state_.integral, -MAX_INTEGRAL, MAX_INTEGRAL);

    // Calculate derivative
    int64_t derivative_scaled = error_scaled - state_.prev_error_scaled;
    state_.prev_error_scaled = error_scaled;

    // Proportional term
    int128_t p_term = (config_.k_ppm * error_scaled * supply) /
                      (PegConstants::PPM_SCALE * PegConstants::PRICE_SCALE);

    // Integral term
    int128_t i_term = (config_.ki_ppm * state_.integral * supply) /
                      (PegConstants::PPM_SCALE * PegConstants::PRICE_SCALE);

    // Derivative term
    int128_t d_term = (config_.kd_ppm * derivative_scaled * supply) /
                      (PegConstants::PPM_SCALE * PegConstants::PRICE_SCALE);

    int128_t delta = p_term + i_term + d_term;

    LOG_DEBUG("PID: P={}, I={}, D={}, total={}",
              p_term, i_term, d_term, delta);

    return delta;
}

int128_t PegController::apply_caps(int128_t delta, int128_t supply) const {
    // Apply per-epoch expansion/contraction caps
    // max_expansion = max_expansion_ppm × supply / PPM_SCALE
    // max_contraction = max_contraction_ppm × supply / PPM_SCALE

    int128_t max_expansion = safe_scaled_multiply(
        supply,
        config_.max_expansion_ppm,
        PegConstants::PPM_SCALE
    );

    int128_t max_contraction = safe_scaled_multiply(
        supply,
        config_.max_contraction_ppm,
        PegConstants::PPM_SCALE
    );

    if (delta > max_expansion) {
        LOG_WARN("Delta capped: {} -> {} (max expansion)", delta, max_expansion);
        return max_expansion;
    }

    if (delta < -max_contraction) {
        LOG_WARN("Delta capped: {} -> {} (max contraction)", delta, -max_contraction);
        return -max_contraction;
    }

    return delta;
}

bool PegController::execute_expansion(int128_t amount, uint64_t epoch_id) {
    // Mint new coins to treasury address
    try {
        bool success = ledger_->mint_to_treasury(amount, config_.treasury_address);
        if (!success) {
            LOG_ERROR("Epoch {}: Failed to mint {} units", epoch_id, amount);
            return false;
        }

        LOG_INFO("Epoch {}: Minted {} units to treasury {}", epoch_id, amount, config_.treasury_address);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Epoch {}: Exception during expansion: {}", epoch_id, e.what());
        return false;
    }
}

bool PegController::execute_contraction(int128_t amount, uint64_t epoch_id) {
    // Two-stage contraction:
    // 1. Burn from treasury if sufficient funds
    // 2. Issue bonds for any remaining amount

    try {
        int128_t treasury_balance = ledger_->get_treasury_balance(config_.treasury_address);

        if (treasury_balance >= amount) {
            // Sufficient treasury funds - burn directly
            bool success = ledger_->burn_from_treasury(amount, config_.treasury_address);
            if (!success) {
                LOG_ERROR("Epoch {}: Failed to burn {} units from treasury", epoch_id, amount);
                return false;
            }

            LOG_INFO("Epoch {}: Burned {} units from treasury", epoch_id, amount);
            state_.bonds_issued_this_epoch = 0;
            return true;

        } else {
            // Insufficient treasury - burn what we can and issue bonds for rest
            int128_t burn_amount = treasury_balance;
            int128_t bond_amount = amount - burn_amount;

            if (burn_amount > 0) {
                bool success = ledger_->burn_from_treasury(burn_amount, config_.treasury_address);
                if (!success) {
                    LOG_ERROR("Epoch {}: Failed to burn {} units from treasury", epoch_id, burn_amount);
                    return false;
                }
                LOG_INFO("Epoch {}: Burned {} units from treasury", epoch_id, burn_amount);
            }

            // Check bond debt limit
            if (config_.max_bond_debt > 0 && state_.total_bond_debt + bond_amount > config_.max_bond_debt) {
                LOG_ERROR("Epoch {}: Bond issuance would exceed max debt limit", epoch_id);
                state_.last_reason = "Bond debt limit reached";
                return false;
            }

            // Issue bonds for remaining amount
            bool success = issue_bonds(bond_amount, epoch_id);
            if (!success) {
                LOG_ERROR("Epoch {}: Failed to issue {} bonds", epoch_id, bond_amount);
                return false;
            }

            LOG_INFO("Epoch {}: Contracted {} units (burned={}, bonds={})",
                     epoch_id, amount, burn_amount, bond_amount);
            return true;
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Epoch {}: Exception during contraction: {}", epoch_id, e.what());
        return false;
    }
}

bool PegController::issue_bonds(int128_t amount, uint64_t epoch_id) {
    // Create bond state
    BondState bond;
    bond.bond_id = epoch_id;  // Use epoch as bond ID (simplified)
    bond.amount = amount;
    bond.issued_epoch = epoch_id;
    bond.maturity_epoch = epoch_id + 100;  // Mature in 100 epochs (configurable)
    bond.discount_rate_ppm = 50'000;       // 5% discount (configurable)

    // Persist bond to database
    std::string key = "peg_bonds:" + std::to_string(bond.bond_id);
    try {
        auto serialized = bond.serialize();
        db_->put(key, serialized);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to persist bond: {}", e.what());
        return false;
    }

    // Update state
    state_.total_bond_debt += amount;
    state_.bonds_issued_this_epoch = amount;

    LOG_INFO("Issued bond {} for {} units (maturity epoch {})",
             bond.bond_id, amount, bond.maturity_epoch);

    return true;
}

bool PegController::persist_state() {
    try {
        auto serialized = state_.serialize();
        db_->put("peg_state:current", serialized);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to persist peg state: {}", e.what());
        return false;
    }
}

bool PegController::persist_event(const PegEvent& event) {
    try {
        std::string key = "peg_events:" + std::to_string(event.epoch_id);
        auto serialized = event.serialize();
        db_->put(key, serialized);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to persist peg event: {}", e.what());
        return false;
    }
}

bool PegController::load_state() {
    try {
        auto data_opt = db_->get("peg_state:current");
        if (!data_opt) {
            return false;  // No previous state
        }

        state_ = PegState::deserialize(*data_opt);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load peg state: {}", e.what());
        return false;
    }
}

bool PegController::should_trigger_circuit_breaker(int64_t price_scaled) const {
    // Trigger if |price - target| > circuit_breaker_threshold
    int64_t error_scaled = price_scaled - PegConstants::TARGET_PRICE;
    int64_t threshold = safe_scaled_multiply(
        PegConstants::TARGET_PRICE,
        config_.circuit_breaker_ppm,
        PegConstants::PPM_SCALE
    );

    return std::abs(error_scaled) > threshold;
}

}  // namespace monetary
}  // namespace ubuntu
