/**
 * @file peg_simulator.cpp
 * @brief Peg mechanism simulator for parameter tuning and scenario testing
 *
 * Usage:
 *   ./peg_simulator --scenario <name> --k <value> --epochs <count>
 *
 * Scenarios:
 *   - stable: Price stays at $1.00
 *   - spike: Price spikes to $1.50 then returns
 *   - drift: Price gradually drifts from $1.00 to $1.10
 *   - random: Random walk around $1.00
 *   - cycle: Cyclical price movement
 *
 * Example:
 *   ./peg_simulator --scenario spike --k 0.05 --epochs 100
 */

#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#include "ubuntu/storage/database.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <getopt.h>

using namespace ubuntu::monetary;
using namespace ubuntu::ledger;
using namespace ubuntu::storage;

// ============================================================================
// Simulator Components
// ============================================================================

class SimulatedLedger : public LedgerAdapter {
public:
    SimulatedLedger(int128_t initial_supply)
        : LedgerAdapter(nullptr, nullptr)
        , total_supply_(initial_supply)
        , treasury_balance_(initial_supply / 10)  // 10% in treasury
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
            return false;  // Insufficient treasury
        }
        total_supply_ -= amount;
        treasury_balance_ -= amount;
        return true;
    }

    bool is_healthy() const override { return true; }

    int128_t total_supply_;
    int128_t treasury_balance_;
};

class SimulatedOracle : public IOracle {
public:
    void set_price(double price_usd, uint64_t timestamp) {
        current_price_.price_scaled = static_cast<int64_t>(price_usd * PegConstants::PRICE_SCALE);
        current_price_.timestamp = timestamp;
        current_price_.source = "simulator";
    }

    std::optional<OraclePrice> get_latest_price() override {
        return current_price_;
    }

    std::optional<OraclePrice> get_median_price(size_t count) override {
        return current_price_;
    }

    std::vector<OraclePrice> get_recent_prices(size_t count) override {
        return {current_price_};
    }

private:
    OraclePrice current_price_;
};

// ============================================================================
// Price Scenarios
// ============================================================================

class PriceScenario {
public:
    virtual ~PriceScenario() = default;
    virtual double get_price(uint64_t epoch) const = 0;
    virtual std::string name() const = 0;
};

class StableScenario : public PriceScenario {
public:
    double get_price(uint64_t epoch) const override { return 1.00; }
    std::string name() const override { return "stable"; }
};

class SpikeScenario : public PriceScenario {
public:
    double get_price(uint64_t epoch) const override {
        if (epoch >= 10 && epoch <= 20) {
            return 1.50;  // Spike
        }
        return 1.00;
    }
    std::string name() const override { return "spike"; }
};

class DriftScenario : public PriceScenario {
public:
    double get_price(uint64_t epoch) const override {
        // Gradual drift from $1.00 to $1.10 over 100 epochs
        return 1.00 + (0.10 * std::min(epoch, 100UL) / 100.0);
    }
    std::string name() const override { return "drift"; }
};

class RandomWalkScenario : public PriceScenario {
public:
    RandomWalkScenario() : price_(1.00), rng_(std::random_device{}()) {}

    double get_price(uint64_t epoch) const override {
        // Mutable to update state
        std::normal_distribution<double> dist(0.0, 0.02);  // ±2% std dev
        const_cast<RandomWalkScenario*>(this)->price_ += dist(const_cast<RandomWalkScenario*>(this)->rng_);
        const_cast<RandomWalkScenario*>(this)->price_ = std::clamp(price_, 0.50, 1.50);
        return price_;
    }

    std::string name() const override { return "random"; }

private:
    double price_;
    std::mt19937 rng_;
};

class CyclicalScenario : public PriceScenario {
public:
    double get_price(uint64_t epoch) const override {
        // Sine wave: 1.00 + 0.10 * sin(epoch / 10)
        return 1.00 + 0.10 * std::sin(epoch / 10.0);
    }
    std::string name() const override { return "cyclical"; }
};

// ============================================================================
// Simulator
// ============================================================================

struct SimulationConfig {
    std::string scenario{"stable"};
    double k{0.05};
    double deadband{0.01};
    double max_expansion{0.05};
    double max_contraction{0.05};
    uint64_t epochs{100};
    int128_t initial_supply{1'000'000 * PegConstants::COIN_SCALE};
    std::string output_file{""};
};

class PegSimulator {
public:
    PegSimulator(const SimulationConfig& config)
        : config_(config)
        , ledger_(std::make_shared<SimulatedLedger>(config.initial_supply))
        , oracle_(std::make_shared<SimulatedOracle>())
        , db_(nullptr)  // No persistence needed for simulation
    {
        // Create scenario
        if (config.scenario == "stable") {
            scenario_ = std::make_unique<StableScenario>();
        } else if (config.scenario == "spike") {
            scenario_ = std::make_unique<SpikeScenario>();
        } else if (config.scenario == "drift") {
            scenario_ = std::make_unique<DriftScenario>();
        } else if (config.scenario == "random") {
            scenario_ = std::make_unique<RandomWalkScenario>();
        } else if (config.scenario == "cyclical") {
            scenario_ = std::make_unique<CyclicalScenario>();
        } else {
            throw std::runtime_error("Unknown scenario: " + config.scenario);
        }

        // Configure peg controller
        PegConfig peg_config;
        peg_config.enabled = true;
        peg_config.k_ppm = static_cast<int64_t>(config.k * PegConstants::PPM_SCALE);
        peg_config.deadband_ppm = static_cast<int64_t>(config.deadband * PegConstants::PPM_SCALE);
        peg_config.max_expansion_ppm = static_cast<int64_t>(config.max_expansion * PegConstants::PPM_SCALE);
        peg_config.max_contraction_ppm = static_cast<int64_t>(config.max_contraction * PegConstants::PPM_SCALE);
        peg_config.epoch_seconds = 3600;
        peg_config.treasury_address = "U1simulated";

        controller_ = std::make_unique<PegController>(ledger_, oracle_, db_, peg_config);
    }

    void run() {
        std::cout << "Peg Mechanism Simulator\n";
        std::cout << "=======================\n\n";
        std::cout << "Scenario: " << scenario_->name() << "\n";
        std::cout << "k = " << config_.k << "\n";
        std::cout << "Deadband = " << config_.deadband << "\n";
        std::cout << "Max expansion/contraction = " << config_.max_expansion << "\n";
        std::cout << "Epochs = " << config_.epochs << "\n\n";

        // CSV output
        std::ofstream csv;
        if (!config_.output_file.empty()) {
            csv.open(config_.output_file);
            csv << "epoch,price,supply,delta,action\n";
        }

        // Print header
        std::cout << std::setw(6) << "Epoch"
                  << std::setw(10) << "Price"
                  << std::setw(18) << "Supply"
                  << std::setw(18) << "Delta"
                  << std::setw(15) << "Action\n";
        std::cout << std::string(67, '-') << "\n";

        // Run simulation
        for (uint64_t epoch = 1; epoch <= config_.epochs; ++epoch) {
            // Set oracle price for this epoch
            double price = scenario_->get_price(epoch);
            oracle_->set_price(price, 1000000 + epoch * 3600);

            // Run epoch
            controller_->run_epoch(epoch, 1000 + epoch, 1000000 + epoch * 3600);

            // Get state
            PegState state = controller_->get_state();

            // Print results
            std::cout << std::setw(6) << epoch
                      << std::setw(10) << std::fixed << std::setprecision(4) << price
                      << std::setw(18) << state.last_supply
                      << std::setw(18) << state.last_delta
                      << std::setw(15) << state.last_action << "\n";

            // Write CSV
            if (csv.is_open()) {
                csv << epoch << "," << price << "," << state.last_supply << ","
                    << state.last_delta << "," << state.last_action << "\n";
            }

            // Check for circuit breaker
            if (state.circuit_breaker_triggered) {
                std::cout << "\n*** CIRCUIT BREAKER TRIGGERED ***\n";
                break;
            }
        }

        std::cout << "\nSimulation complete.\n";

        if (csv.is_open()) {
            csv.close();
            std::cout << "Results written to: " << config_.output_file << "\n";
        }

        // Print summary statistics
        print_summary();
    }

private:
    void print_summary() {
        std::cout << "\nSummary Statistics:\n";
        std::cout << "===================\n";

        auto events = controller_->get_recent_events(config_.epochs);

        int expand_count = 0;
        int contract_count = 0;
        int deadband_count = 0;

        for (const auto& event : events) {
            if (event.action == "expand") expand_count++;
            else if (event.action == "contract") contract_count++;
            else if (event.action == "deadband") deadband_count++;
        }

        std::cout << "Expansions: " << expand_count << "\n";
        std::cout << "Contractions: " << contract_count << "\n";
        std::cout << "Dead-band (no action): " << deadband_count << "\n";

        PegState final_state = controller_->get_state();
        int128_t initial_supply = config_.initial_supply;
        int128_t final_supply = final_state.last_supply;
        double supply_change_pct = 100.0 * static_cast<double>(final_supply - initial_supply) /
                                   static_cast<double>(initial_supply);

        std::cout << "Initial supply: " << initial_supply << "\n";
        std::cout << "Final supply: " << final_supply << "\n";
        std::cout << "Supply change: " << std::fixed << std::setprecision(2)
                  << supply_change_pct << "%\n";
    }

    SimulationConfig config_;
    std::shared_ptr<SimulatedLedger> ledger_;
    std::shared_ptr<SimulatedOracle> oracle_;
    std::shared_ptr<Database> db_;
    std::unique_ptr<PriceScenario> scenario_;
    std::unique_ptr<PegController> controller_;
};

// ============================================================================
// Main
// ============================================================================

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --scenario <name>     Price scenario (stable, spike, drift, random, cyclical)\n";
    std::cout << "  --k <value>           Proportional gain (default: 0.05)\n";
    std::cout << "  --deadband <value>    Dead-band percentage (default: 0.01)\n";
    std::cout << "  --max-change <value>  Max expansion/contraction per epoch (default: 0.05)\n";
    std::cout << "  --epochs <count>      Number of epochs to simulate (default: 100)\n";
    std::cout << "  --output <file>       CSV output file (optional)\n";
    std::cout << "  --help                Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " --scenario spike --k 0.05 --epochs 50\n";
    std::cout << "  " << program << " --scenario random --k 0.10 --output results.csv\n";
}

int main(int argc, char* argv[]) {
    SimulationConfig config;

    // Parse command line arguments
    static struct option long_options[] = {
        {"scenario", required_argument, 0, 's'},
        {"k", required_argument, 0, 'k'},
        {"deadband", required_argument, 0, 'd'},
        {"max-change", required_argument, 0, 'm'},
        {"epochs", required_argument, 0, 'e'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:k:d:m:e:o:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 's':
                config.scenario = optarg;
                break;
            case 'k':
                config.k = std::stod(optarg);
                break;
            case 'd':
                config.deadband = std::stod(optarg);
                break;
            case 'm':
                config.max_expansion = config.max_contraction = std::stod(optarg);
                break;
            case 'e':
                config.epochs = std::stoull(optarg);
                break;
            case 'o':
                config.output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    try {
        PegSimulator simulator(config);
        simulator.run();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
