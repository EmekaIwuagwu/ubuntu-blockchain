#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#include "ubuntu/storage/database.h"

using namespace ubuntu::monetary;
using namespace ubuntu::ledger;
using namespace ubuntu::storage;

using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

// ============================================================================
// Mock Classes
// ============================================================================

class MockOracle : public IOracle {
public:
    MOCK_METHOD(std::optional<OraclePrice>, get_latest_price, (), (override));
    MOCK_METHOD(std::optional<OraclePrice>, get_median_price, (size_t), (override));
    MOCK_METHOD(std::vector<OraclePrice>, get_recent_prices, (size_t), (override));

    void set_price(double price_usd) {
        OraclePrice price;
        price.price_scaled = static_cast<int64_t>(price_usd * PegConstants::PRICE_SCALE);
        price.timestamp = 1234567890;
        price.source = "mock";
        current_price_ = price;
    }

    OraclePrice current_price_;
};

class MockLedger : public LedgerAdapter {
public:
    MockLedger()
        : LedgerAdapter(nullptr, nullptr)
        , total_supply_(0)
        , treasury_balance_(0)
    {}

    int128_t get_total_supply() const override {
        return total_supply_;
    }

    int128_t get_treasury_balance(const std::string& address) const override {
        return treasury_balance_;
    }

    bool mint_to_treasury(int128_t amount, const std::string& address) override {
        total_supply_ += amount;
        treasury_balance_ += amount;
        return true;
    }

    bool burn_from_treasury(int128_t amount, const std::string& address) override {
        if (treasury_balance_ < amount) {
            return false;
        }
        total_supply_ -= amount;
        treasury_balance_ -= amount;
        return true;
    }

    bool is_healthy() const override {
        return true;
    }

    int128_t total_supply_;
    int128_t treasury_balance_;
};

class MockDatabase : public Database {
public:
    MockDatabase() : Database("") {}

    void put(const std::string& key, const std::vector<uint8_t>& value) override {
        store_[key] = value;
    }

    std::optional<std::vector<uint8_t>> get(const std::string& key) override {
        auto it = store_.find(key);
        if (it != store_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void remove(const std::string& key) override {
        store_.erase(key);
    }

    std::unordered_map<std::string, std::vector<uint8_t>> store_;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class PegControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ledger_ = std::make_shared<MockLedger>();
        oracle_ = std::make_shared<NiceMock<MockOracle>>();
        db_ = std::make_shared<MockDatabase>();

        // Default configuration
        config_.enabled = true;
        config_.epoch_seconds = 3600;
        config_.deadband_ppm = 10'000;     // 1%
        config_.k_ppm = 50'000;            // 5%
        config_.max_expansion_ppm = 50'000;   // 5%
        config_.max_contraction_ppm = 50'000; // 5%
        config_.oracle_max_age_seconds = 600;
        config_.circuit_breaker_ppm = 500'000; // 50%
        config_.treasury_address = "U1qyqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp3w4s";

        // Set initial supply
        ledger_->total_supply_ = 1'000'000 * PegConstants::COIN_SCALE;  // 1M UBU
        ledger_->treasury_balance_ = 100'000 * PegConstants::COIN_SCALE;  // 100K in treasury

        // Default oracle price: $1.00
        oracle_->set_price(1.00);
        ON_CALL(*oracle_, get_latest_price())
            .WillByDefault([this]() { return oracle_->current_price_; });
    }

    std::shared_ptr<MockLedger> ledger_;
    std::shared_ptr<MockOracle> oracle_;
    std::shared_ptr<MockDatabase> db_;
    PegConfig config_;
};

// ============================================================================
// State Serialization Tests
// ============================================================================

TEST(PegStateTest, SerializationRoundTrip) {
    PegState original;
    original.epoch_id = 123;
    original.timestamp = 1234567890;
    original.block_height = 100000;
    original.last_price_scaled = 1'050'000;  // $1.05
    original.last_supply = 1'000'000;
    original.last_delta = 50'000;
    original.total_bond_debt = 10'000;
    original.last_action = "expand";
    original.last_reason = "Price too high";
    original.circuit_breaker_triggered = false;

    // Serialize
    auto serialized = original.serialize();
    ASSERT_GT(serialized.size(), 0);

    // Deserialize
    PegState deserialized = PegState::deserialize(serialized);

    // Verify
    EXPECT_EQ(deserialized.epoch_id, original.epoch_id);
    EXPECT_EQ(deserialized.timestamp, original.timestamp);
    EXPECT_EQ(deserialized.block_height, original.block_height);
    EXPECT_EQ(deserialized.last_price_scaled, original.last_price_scaled);
    EXPECT_EQ(deserialized.last_supply, original.last_supply);
    EXPECT_EQ(deserialized.last_delta, original.last_delta);
    EXPECT_EQ(deserialized.total_bond_debt, original.total_bond_debt);
    EXPECT_EQ(deserialized.last_action, original.last_action);
    EXPECT_EQ(deserialized.last_reason, original.last_reason);
    EXPECT_EQ(deserialized.circuit_breaker_triggered, original.circuit_breaker_triggered);
}

// ============================================================================
// Dead-band Tests
// ============================================================================

TEST_F(PegControllerTest, PriceWithinDeadband_NoAction) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set price within dead-band: $1.005 (0.5% deviation, within 1% dead-band)
    oracle_->set_price(1.005);

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();
    EXPECT_EQ(state.last_action, "deadband");
    EXPECT_EQ(state.last_delta, 0);

    // Supply should be unchanged
    EXPECT_EQ(ledger_->total_supply_, 1'000'000 * PegConstants::COIN_SCALE);
}

TEST_F(PegControllerTest, PriceAboveDeadband_Expands) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set price above dead-band: $1.05 (5% deviation)
    oracle_->set_price(1.05);

    int128_t initial_supply = ledger_->total_supply_;

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();
    EXPECT_EQ(state.last_action, "expand");
    EXPECT_GT(state.last_delta, 0);

    // Supply should increase
    EXPECT_GT(ledger_->total_supply_, initial_supply);
}

TEST_F(PegControllerTest, PriceBelowDeadband_Contracts) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set price below dead-band: $0.95 (5% deviation)
    oracle_->set_price(0.95);

    int128_t initial_supply = ledger_->total_supply_;

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();
    EXPECT_EQ(state.last_action, "contract");
    EXPECT_LT(state.last_delta, 0);

    // Supply should decrease
    EXPECT_LT(ledger_->total_supply_, initial_supply);
}

// ============================================================================
// Proportional Controller Tests
// ============================================================================

TEST_F(PegControllerTest, ProportionalController_CorrectDelta) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Price = $1.05, k = 0.05, supply = 1M
    // Expected delta = 0.05 × 0.05 × 1M = 2,500 UBU
    oracle_->set_price(1.05);

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();

    // Calculate expected delta
    int64_t error = 50'000;  // 0.05 in scaled units (1.05 - 1.00 = 0.05)
    int128_t expected_delta = (config_.k_ppm * error * ledger_->total_supply_) /
                              (PegConstants::PPM_SCALE * PegConstants::PRICE_SCALE);

    EXPECT_NEAR(static_cast<double>(state.last_delta),
                static_cast<double>(expected_delta),
                1.0);  // Allow 1 unit tolerance for rounding
}

// ============================================================================
// Caps Tests
// ============================================================================

TEST_F(PegControllerTest, ExpansionCap_Applied) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set very high price to trigger cap
    oracle_->set_price(2.00);  // 100% deviation

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();

    // Delta should be capped at 5% of supply
    int128_t max_expansion = (config_.max_expansion_ppm * ledger_->total_supply_) /
                             PegConstants::PPM_SCALE;
    EXPECT_LE(state.last_delta, max_expansion);
}

TEST_F(PegControllerTest, ContractionCap_Applied) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set very low price to trigger cap
    oracle_->set_price(0.50);  // 50% deviation

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();

    // Delta should be capped at -5% of supply
    int128_t max_contraction = (config_.max_contraction_ppm * ledger_->total_supply_) /
                               PegConstants::PPM_SCALE;
    EXPECT_GE(state.last_delta, -max_contraction);
}

// ============================================================================
// Circuit Breaker Tests
// ============================================================================

TEST_F(PegControllerTest, CircuitBreaker_TriggersOnExtremePrice) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Set price beyond circuit breaker threshold (50%)
    oracle_->set_price(1.60);  // 60% deviation

    bool success = controller.run_epoch(1, 1000, 1234567890);
    ASSERT_TRUE(success);

    PegState state = controller.get_state();
    EXPECT_TRUE(state.circuit_breaker_triggered);
    EXPECT_EQ(state.last_action, "circuit_breaker");
}

TEST_F(PegControllerTest, CircuitBreaker_PreventsSubsequentActions) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Trigger circuit breaker
    oracle_->set_price(1.60);
    controller.run_epoch(1, 1000, 1234567890);

    // Reset to normal price
    oracle_->set_price(1.05);

    // Run another epoch - should not take action
    int128_t supply_before = ledger_->total_supply_;
    controller.run_epoch(2, 1001, 1234567900);
    int128_t supply_after = ledger_->total_supply_;

    EXPECT_EQ(supply_before, supply_after);  // No change
}

// ============================================================================
// State Persistence Tests
// ============================================================================

TEST_F(PegControllerTest, StatePersistence_SavesAndRestores) {
    {
        PegController controller(ledger_, oracle_, db_, config_);
        oracle_->set_price(1.05);
        controller.run_epoch(1, 1000, 1234567890);
    }

    // Create new controller with same database
    PegController controller2(ledger_, oracle_, db_, config_);
    PegState state = controller2.get_state();

    // State should be restored
    EXPECT_EQ(state.epoch_id, 1);
    EXPECT_EQ(state.last_action, "expand");
}

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_F(PegControllerTest, IsHealthy_WhenEnabled) {
    PegController controller(ledger_, oracle_, db_, config_);
    EXPECT_TRUE(controller.is_healthy());
}

TEST_F(PegControllerTest, IsHealthy_FalseWhenDisabled) {
    config_.enabled = false;
    PegController controller(ledger_, oracle_, db_, config_);
    EXPECT_FALSE(controller.is_healthy());
}

TEST_F(PegControllerTest, IsHealthy_FalseWhenCircuitBreaker) {
    PegController controller(ledger_, oracle_, db_, config_);
    oracle_->set_price(1.60);
    controller.run_epoch(1, 1000, 1234567890);

    EXPECT_FALSE(controller.is_healthy());
}

// ============================================================================
// Event History Tests
// ============================================================================

TEST_F(PegControllerTest, EventHistory_Persisted) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Run multiple epochs
    for (uint64_t i = 1; i <= 5; ++i) {
        oracle_->set_price(1.0 + (i * 0.01));  // Vary price
        controller.run_epoch(i, 1000 + i, 1234567890 + i * 3600);
    }

    // Retrieve history
    auto events = controller.get_recent_events(10);
    EXPECT_EQ(events.size(), 5);

    // Events should be in reverse order (newest first)
    EXPECT_EQ(events[0].epoch_id, 5);
    EXPECT_EQ(events[4].epoch_id, 1);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
