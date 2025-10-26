# Peg Module Integration Patch

This document shows how to integrate the peg module into the Ubuntu Blockchain daemon (`ubud`).

## Integration Points

### 1. Include Headers

Add to `src/daemon/ubud.cpp`:

```cpp
#ifdef ENABLE_PEG
#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#endif
```

### 2. Daemon Class Members

Add to the `UbuntuDaemon` class in `src/daemon/ubud.cpp`:

```cpp
class UbuntuDaemon {
public:
    // ... existing members ...

#ifdef ENABLE_PEG
    std::shared_ptr<ubuntu::monetary::PegController> peg_controller_;
    std::shared_ptr<ubuntu::ledger::LedgerAdapter> ledger_adapter_;
    std::shared_ptr<ubuntu::monetary::IOracle> oracle_;
    uint64_t last_peg_epoch_time_{0};
    uint64_t peg_epoch_counter_{0};
#endif

private:
    // ... existing methods ...

#ifdef ENABLE_PEG
    void initialize_peg_module();
    void check_and_run_peg_epoch();
#endif
};
```

### 3. Initialization Method

Add after daemon initialization (in `UbuntuDaemon::initialize()`):

```cpp
void UbuntuDaemon::initialize() {
    // ... existing initialization code ...

#ifdef ENABLE_PEG
    initialize_peg_module();
#endif

    LOG_INFO("Ubuntu Blockchain daemon initialized successfully");
}

#ifdef ENABLE_PEG
void UbuntuDaemon::initialize_peg_module() {
    using namespace ubuntu::monetary;

    // Read configuration
    bool peg_enabled = config_->get_bool("peg.enabled", false);
    if (!peg_enabled) {
        LOG_INFO("Peg module disabled in configuration");
        return;
    }

    // Create ledger adapter
    ledger_adapter_ = std::make_shared<ubuntu::ledger::LedgerAdapter>(
        blockchain_,
        utxo_db_
    );

    // Create oracle
    std::string oracle_type = config_->get_string("peg.oracle_type", "stub");
    std::string oracle_config = config_->get_string("peg.oracle_config", "fixed:1.00");
    oracle_ = OracleFactory::create(oracle_type, oracle_config);

    // Create peg configuration
    PegConfig peg_config;
    peg_config.enabled = true;
    peg_config.epoch_seconds = config_->get_uint64("peg.epoch_seconds", 3600);
    peg_config.deadband_ppm = config_->get_int64("peg.deadband_ppm", 10'000);
    peg_config.k_ppm = config_->get_int64("peg.k_ppm", 50'000);
    peg_config.max_expansion_ppm = config_->get_int64("peg.max_expansion_ppm", 50'000);
    peg_config.max_contraction_ppm = config_->get_int64("peg.max_contraction_ppm", 50'000);
    peg_config.ki_ppm = config_->get_int64("peg.ki_ppm", 0);
    peg_config.kd_ppm = config_->get_int64("peg.kd_ppm", 0);
    peg_config.oracle_max_age_seconds = config_->get_uint64("peg.oracle_max_age_seconds", 600);
    peg_config.circuit_breaker_ppm = config_->get_int64("peg.circuit_breaker_ppm", 500'000);
    peg_config.treasury_address = config_->get_string("peg.treasury_address", "");

    if (peg_config.treasury_address.empty()) {
        LOG_ERROR("Peg module requires treasury_address in configuration");
        return;
    }

    // Create peg controller
    peg_controller_ = std::make_shared<PegController>(
        ledger_adapter_,
        oracle_,
        db_,
        peg_config
    );

    // Register RPC methods
    register_peg_rpc_methods(*rpc_server_, peg_controller_);

    LOG_INFO("Peg module initialized successfully");
    LOG_INFO("  Treasury: {}", peg_config.treasury_address);
    LOG_INFO("  Epoch seconds: {}", peg_config.epoch_seconds);
    LOG_INFO("  k: {:.4f}", static_cast<double>(peg_config.k_ppm) / 1'000'000);
}
#endif
```

### 4. Epoch Scheduler Hook

Add to the daemon's main loop (in `UbuntuDaemon::run()`):

```cpp
void UbuntuDaemon::run() {
    LOG_INFO("Starting Ubuntu Blockchain daemon...");

    while (!shutdown_requested_) {
        try {
            // Process network messages
            network_manager_->poll();

            // Update mempool
            mempool_->periodic_cleanup();

            // Update blockchain
            blockchain_->update();

#ifdef ENABLE_PEG
            // Check and run peg epoch if needed
            check_and_run_peg_epoch();
#endif

            // Sleep to prevent busy-wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            LOG_ERROR("Error in main loop: {}", e.what());
        }
    }

    LOG_INFO("Daemon shutdown complete");
}

#ifdef ENABLE_PEG
void UbuntuDaemon::check_and_run_peg_epoch() {
    if (!peg_controller_) {
        return;  // Peg module not initialized
    }

    // Get current time
    uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Get epoch duration from config
    auto config = peg_controller_->get_config();
    uint64_t epoch_seconds = config.epoch_seconds;

    // Check if epoch should run
    if (last_peg_epoch_time_ == 0) {
        // First epoch
        last_peg_epoch_time_ = current_time;
        return;
    }

    uint64_t time_since_last = current_time - last_peg_epoch_time_;
    if (time_since_last >= epoch_seconds) {
        // Time to run epoch
        peg_epoch_counter_++;
        uint32_t block_height = blockchain_->get_height();

        LOG_INFO("Running peg epoch {} at block height {}", peg_epoch_counter_, block_height);

        bool success = peg_controller_->run_epoch(
            peg_epoch_counter_,
            block_height,
            current_time
        );

        if (success) {
            auto state = peg_controller_->get_state();
            LOG_INFO("Peg epoch {} complete: action={}, delta={}",
                     peg_epoch_counter_, state.last_action, state.last_delta);
        } else {
            LOG_ERROR("Peg epoch {} failed", peg_epoch_counter_);
        }

        // Update last epoch time
        last_peg_epoch_time_ = current_time;
    }
}
#endif
```

### 5. Shutdown Cleanup

Add to `UbuntuDaemon::shutdown()`:

```cpp
void UbuntuDaemon::shutdown() {
    LOG_INFO("Shutting down daemon...");

#ifdef ENABLE_PEG
    // Shutdown peg controller (will persist final state)
    peg_controller_.reset();
    ledger_adapter_.reset();
    oracle_.reset();
#endif

    // ... existing shutdown code ...
}
```

## Complete Integration Example

Here's a minimal `ubud.cpp` skeleton showing full integration:

```cpp
#include "ubuntu/core/chain.h"
#include "ubuntu/network/network_manager.h"
#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/config/config.h"

#ifdef ENABLE_PEG
#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/ledger/ledger_adapter.h"
#endif

class UbuntuDaemon {
public:
    UbuntuDaemon(const std::string& config_file)
        : config_(std::make_shared<Config>(config_file))
    {}

    void initialize() {
        // Initialize core components
        db_ = std::make_shared<RocksDatabase>("./data");
        blockchain_ = std::make_shared<Blockchain>(db_);
        utxo_db_ = std::make_shared<UTXODatabase>(db_);
        mempool_ = std::make_shared<Mempool>(blockchain_);
        network_manager_ = std::make_shared<NetworkManager>();
        rpc_server_ = std::make_shared<RpcServer>("127.0.0.1", 8332);

#ifdef ENABLE_PEG
        initialize_peg_module();
#endif

        LOG_INFO("Daemon initialized");
    }

    void run() {
        while (!shutdown_requested_) {
            network_manager_->poll();
            mempool_->periodic_cleanup();

#ifdef ENABLE_PEG
            check_and_run_peg_epoch();
#endif

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    std::shared_ptr<Config> config_;
    std::shared_ptr<Database> db_;
    std::shared_ptr<Blockchain> blockchain_;
    std::shared_ptr<UTXODatabase> utxo_db_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<NetworkManager> network_manager_;
    std::shared_ptr<RpcServer> rpc_server_;
    bool shutdown_requested_{false};

#ifdef ENABLE_PEG
    std::shared_ptr<ubuntu::monetary::PegController> peg_controller_;
    std::shared_ptr<ubuntu::ledger::LedgerAdapter> ledger_adapter_;
    std::shared_ptr<ubuntu::monetary::IOracle> oracle_;
    uint64_t last_peg_epoch_time_{0};
    uint64_t peg_epoch_counter_{0};

    void initialize_peg_module() {
        // See section 3 above
    }

    void check_and_run_peg_epoch() {
        // See section 4 above
    }
#endif
};

int main(int argc, char* argv[]) {
    try {
        UbuntuDaemon daemon("ubuntu.conf");
        daemon.initialize();
        daemon.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

## Configuration File Example

Add to `ubuntu.conf`:

```ini
[peg]
enabled=1
epoch_seconds=3600
deadband_ppm=10000
k_ppm=50000
max_expansion_ppm=50000
max_contraction_ppm=50000
oracle_type=stub
oracle_config=fixed:1.00
oracle_max_age_seconds=600
circuit_breaker_ppm=500000
treasury_address=U1qyqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp3w4s
```

## Build Instructions

```bash
# Configure with peg module enabled
cmake -B build -DENABLE_PEG=ON

# Build
cmake --build build

# The daemon will now include peg functionality
./build/bin/ubud
```

## Verification

After integration, verify:

1. **Daemon starts successfully:**
   ```bash
   ./ubud
   # Check logs for "Peg module initialized successfully"
   ```

2. **RPC endpoints work:**
   ```bash
   ubu-cli peg_getstatus
   ubu-cli peg_gethistory 10
   ```

3. **Epochs execute:**
   - Watch logs for "Running peg epoch" messages
   - Verify epoch interval matches configuration

## Notes

- The peg module is **fully optional** - set `ENABLE_PEG=OFF` to exclude it
- All peg code is guarded by `#ifdef ENABLE_PEG` preprocessor directives
- Zero performance impact when disabled (code not compiled)
- No changes to consensus, transactions, or existing RPC methods required
