#include <gtest/gtest.h>

#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#include "ubuntu/storage/database.h"
#include "ubuntu/core/logger.h"

#include <filesystem>
#include <memory>
#include <thread>
#include <chrono>

using namespace ubuntu::monetary;
using namespace ubuntu::ledger;
using namespace ubuntu::storage;

// ============================================================================
// Integration Test Fixture
// ============================================================================

class PegIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        test_dir_ = "/tmp/peg_test_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);

        // Initialize components
        db_ = std::make_shared<RocksDatabase>(test_dir_);

        // Note: In real integration, these would be actual blockchain components
        // For testing, we use simplified versions
        blockchain_ = create_test_blockchain();
        utxo_db_ = create_test_utxo_db();

        ledger_ = std::make_shared<LedgerAdapter>(blockchain_, utxo_db_);
        oracle_ = OracleFactory::create("stub", "fixed:1.00");

        // Configure peg
        config_.enabled = true;
        config_.epoch_seconds = 60;  // Short epochs for testing
        config_.deadband_ppm = 10'000;     // 1%
        config_.k_ppm = 50'000;            // 5%
        config_.max_expansion_ppm = 100'000;   // 10%
        config_.max_contraction_ppm = 100'000; // 10%
        config_.oracle_max_age_seconds = 600;
        config_.circuit_breaker_ppm = 500'000; // 50%
        config_.treasury_address = "U1qyqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp3w4s";

        LOG_INFO("Integration test setup complete");
    }

    void TearDown() override {
        // Cleanup test directory
        std::filesystem::remove_all(test_dir_);
    }

    // Helper: Create test blockchain instance
    std::shared_ptr<core::Blockchain> create_test_blockchain() {
        // Simplified blockchain for testing
        // In production, this would be the real Blockchain class
        return nullptr;  // Placeholder
    }

    // Helper: Create test UTXO database
    std::shared_ptr<storage::UTXODatabase> create_test_utxo_db() {
        // Simplified UTXO DB for testing
        return nullptr;  // Placeholder
    }

    std::string test_dir_;
    std::shared_ptr<Database> db_;
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<storage::UTXODatabase> utxo_db_;
    std::shared_ptr<LedgerAdapter> ledger_;
    std::shared_ptr<IOracle> oracle_;
    PegConfig config_;
};

// ============================================================================
// End-to-End Scenario Tests
// ============================================================================

TEST_F(PegIntegrationTest, DISABLED_FullCycle_PriceRecovery) {
    // NOTE: Disabled because it requires full blockchain integration
    // Enable when blockchain and UTXO components are available

    PegController controller(ledger_, oracle_, db_, config_);

    // Scenario: Price starts at $1.00, rises to $1.10, then recovers to $1.00

    // Epoch 1: Price = $1.00 (stable)
    auto oracle_stub = std::dynamic_pointer_cast<OracleStub>(oracle_);
    oracle_stub->set_fixed_price(1.00);
    controller.run_epoch(1, 1000, 1000000);

    PegState state1 = controller.get_state();
    EXPECT_EQ(state1.last_action, "deadband");

    // Epoch 2: Price rises to $1.10 (expansion needed)
    oracle_stub->set_fixed_price(1.10);
    controller.run_epoch(2, 1001, 1003600);

    PegState state2 = controller.get_state();
    EXPECT_EQ(state2.last_action, "expand");
    EXPECT_GT(state2.last_delta, 0);

    // Epoch 3: Price returns to $1.00 (deadband)
    oracle_stub->set_fixed_price(1.00);
    controller.run_epoch(3, 1002, 1007200);

    PegState state3 = controller.get_state();
    EXPECT_EQ(state3.last_action, "deadband");
}

TEST_F(PegIntegrationTest, DISABLED_StatePersistence_AcrossRestarts) {
    // NOTE: Disabled because it requires full blockchain integration

    uint64_t final_epoch;
    int128_t final_supply;

    {
        // First controller instance
        PegController controller(ledger_, oracle_, db_, config_);

        for (uint64_t i = 1; i <= 10; ++i) {
            controller.run_epoch(i, 1000 + i, 1000000 + i * 3600);
        }

        PegState state = controller.get_state();
        final_epoch = state.epoch_id;
        final_supply = state.last_supply;
    }

    {
        // Second controller instance (simulates restart)
        PegController controller2(ledger_, oracle_, db_, config_);

        PegState restored_state = controller2.get_state();
        EXPECT_EQ(restored_state.epoch_id, final_epoch);
        EXPECT_EQ(restored_state.last_supply, final_supply);
    }
}

// ============================================================================
// Stress Test
// ============================================================================

TEST_F(PegIntegrationTest, DISABLED_StressTest_100Epochs) {
    // NOTE: Disabled because it requires full blockchain integration

    PegController controller(ledger_, oracle_, db_, config_);

    // Run 100 epochs with random price variations
    auto oracle_stub = std::dynamic_pointer_cast<OracleStub>(oracle_);
    oracle_ = OracleFactory::create("stub", "random:1.00:0.10");  // ±10% variance

    for (uint64_t epoch = 1; epoch <= 100; ++epoch) {
        bool success = controller.run_epoch(
            epoch,
            1000 + epoch,
            1000000 + epoch * 3600
        );

        ASSERT_TRUE(success) << "Epoch " << epoch << " failed";

        // Verify state is valid
        PegState state = controller.get_state();
        EXPECT_EQ(state.epoch_id, epoch);
        EXPECT_FALSE(state.circuit_breaker_triggered);
    }

    // Retrieve full history
    auto events = controller.get_recent_events(100);
    EXPECT_EQ(events.size(), 100);
}

// ============================================================================
// RPC Integration Test
// ============================================================================

TEST_F(PegIntegrationTest, DISABLED_RpcIntegration_GetStatus) {
    // NOTE: Disabled because it requires RPC server integration

    PegController controller(ledger_, oracle_, db_, config_);

    // Run some epochs
    for (uint64_t i = 1; i <= 5; ++i) {
        controller.run_epoch(i, 1000 + i, 1000000 + i * 3600);
    }

    // Simulate RPC call to peg_getstatus
    // In production, this would go through the actual RPC server
    PegState state = controller.get_state();
    PegConfig config = controller.get_config();
    bool healthy = controller.is_healthy();

    EXPECT_EQ(state.epoch_id, 5);
    EXPECT_TRUE(config.enabled);
    EXPECT_TRUE(healthy);
}

// ============================================================================
// Configuration Update Test
// ============================================================================

TEST_F(PegIntegrationTest, ConfigurationUpdate_AppliesNextEpoch) {
    PegController controller(ledger_, oracle_, db_, config_);

    // Run epoch with k=5%
    auto oracle_stub = std::dynamic_pointer_cast<OracleStub>(oracle_);
    oracle_stub->set_fixed_price(1.05);
    controller.run_epoch(1, 1000, 1000000);

    PegState state1 = controller.get_state();
    int128_t delta1 = state1.last_delta;

    // Update configuration: change k to 10%
    PegConfig new_config = config_;
    new_config.k_ppm = 100'000;  // 10%
    controller.update_config(new_config);

    // Run another epoch with same price
    controller.run_epoch(2, 1001, 1003600);

    PegState state2 = controller.get_state();
    int128_t delta2 = state2.last_delta;

    // Delta should be roughly double (k doubled)
    // Allow some margin for rounding and supply changes
    double ratio = static_cast<double>(delta2) / static_cast<double>(delta1);
    EXPECT_NEAR(ratio, 2.0, 0.2);
}

// ============================================================================
// Performance Benchmark
// ============================================================================

TEST_F(PegIntegrationTest, Performance_EpochExecutionTime) {
    PegController controller(ledger_, oracle_, db_, config_);

    auto start = std::chrono::high_resolution_clock::now();

    // Run 1000 epochs
    for (uint64_t epoch = 1; epoch <= 1000; ++epoch) {
        controller.run_epoch(epoch, 1000 + epoch, 1000000 + epoch * 3600);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double avg_time_ms = static_cast<double>(duration.count()) / 1000.0;

    LOG_INFO("Performance: 1000 epochs in {} ms (avg {:.2f} ms/epoch)",
             duration.count(), avg_time_ms);

    // Each epoch should complete in reasonable time (< 10ms average)
    EXPECT_LT(avg_time_ms, 10.0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize logging for integration tests
    ubuntu::core::Logger::init(ubuntu::core::LogLevel::INFO);

    return RUN_ALL_TESTS();
}
