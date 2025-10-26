# Ubuntu Blockchain Peg Module

## Overview

The Peg Module implements an algorithmic stablecoin mechanism that maintains a 1 UBU = 1 USD target price through automated supply adjustments. This module is designed as a self-contained, production-quality addition to the Ubuntu Blockchain that operates without modifying consensus rules, transaction formats, or UTXO semantics.

**Key Features:**
- Algorithmic price stabilization using proportional or PID control
- Deterministic integer-only arithmetic for cross-platform compatibility
- Configurable parameters with safe defaults
- Circuit breaker and safety caps
- Full audit trail via RocksDB persistence
- RPC API for monitoring and control

## Architecture

### Components

1. **PegController** (`include/ubuntu/monetary/peg_controller.h`)
   - Main control logic
   - Executes epoch-based supply adjustments
   - Manages state and persistence

2. **Oracle Interface** (`include/ubuntu/monetary/oracle_interface.h`)
   - Abstract interface for price feeds
   - Supports multiple oracle types (stub, file, RPC, on-chain)
   - Built-in staleness checking

3. **Ledger Adapter** (`include/ubuntu/ledger/ledger_adapter.h`)
   - Interface to blockchain for mint/burn operations
   - Query supply and balances
   - Compatible with existing UTXO model

4. **State Management** (`include/ubuntu/monetary/peg_state.h`)
   - Persistent state structures
   - Serialization/deserialization
   - Event logging for audit trail

### Control Algorithm

The peg mechanism uses a **proportional controller** by default:

```
ΔSupply = k × (price - $1.00) × supply
```

Where:
- `k` = proportional gain (default: 0.05 or 5%)
- `price` = current market price from oracle
- `supply` = total circulating supply

**Optional PID enhancement:**
```
ΔSupply = (kp × error + ki × integral + kd × derivative) × supply
```

### Safety Mechanisms

1. **Dead-band** (±1% default)
   - No action taken if price within tolerance
   - Prevents excessive adjustments

2. **Per-epoch caps** (5% default)
   - Maximum expansion: 5% of supply per epoch
   - Maximum contraction: 5% of supply per epoch

3. **Circuit breaker** (50% threshold)
   - Emergency stop if price deviation exceeds threshold
   - Requires manual reset

4. **Oracle staleness** (10 minutes default)
   - Rejects stale price data
   - Prevents acting on outdated information

## Configuration

### ubuntu.conf Settings

Add to `ubuntu.conf`:

```ini
[peg]
# Enable peg mechanism
enabled=1

# Epoch configuration
epoch_seconds=3600              # 1 hour epochs
use_block_epochs=0              # Use time-based (not block-based)

# Controller parameters (in parts per million)
k_ppm=50000                     # 5% proportional gain
deadband_ppm=10000              # 1% dead-band
max_expansion_ppm=50000         # 5% max expansion
max_contraction_ppm=50000       # 5% max contraction

# PID parameters (optional, 0 = disabled)
ki_ppm=0                        # Integral gain
kd_ppm=0                        # Derivative gain

# Oracle settings
oracle_type=stub                # Oracle type (stub, file, rpc)
oracle_config=fixed:1.00        # Oracle configuration
oracle_max_age_seconds=600      # 10 minutes

# Safety
circuit_breaker_ppm=500000      # 50% emergency threshold
max_bond_debt=0                 # 0 = unlimited

# Treasury
treasury_address=U1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

### Parameter Tuning Guidelines

**Proportional Gain (k):**
- Lower (0.01-0.03): Slower response, more stable
- Medium (0.05): Balanced (recommended)
- Higher (0.10-0.20): Faster response, more volatile

**Dead-band:**
- Smaller (0.5%): More responsive, more frequent actions
- Larger (2%): Less responsive, fewer actions

**Epoch Duration:**
- Shorter (15 min): Faster convergence, higher overhead
- Longer (1 hour): Slower convergence, lower overhead

**Use the simulator** to test parameters:
```bash
./peg_simulator --scenario random --k 0.05 --epochs 100 --output results.csv
```

## Operations

### Starting the Peg Module

The peg module is automatically initialized when the daemon starts if `enabled=1` in config.

```bash
# Build with peg support
cmake -DENABLE_PEG=ON ..
make

# Start daemon
./ubud
```

### Monitoring

**RPC API:**

1. **Get Status**
```bash
ubu-cli peg_getstatus
```

Response:
```json
{
  "enabled": true,
  "healthy": true,
  "epoch_id": 1234,
  "timestamp": 1234567890,
  "price_usd": 1.05,
  "supply": "1000000000000000",
  "last_delta": "50000000000",
  "last_action": "expand",
  "circuit_breaker": false,
  "config": {
    "k": 0.05,
    "deadband": 0.01
  }
}
```

2. **Get History**
```bash
ubu-cli peg_gethistory 100
```

Response:
```json
{
  "events": [
    {
      "epoch_id": 1234,
      "price_usd": 1.05,
      "supply": "1000000000000000",
      "delta": "50000000000",
      "action": "expand"
    }
  ]
}
```

### Emergency Procedures

**Circuit Breaker Triggered:**

1. Investigate cause of price deviation
2. Check oracle health and data
3. Review system logs
4. Once resolved, reset via RPC:
```bash
ubu-cli peg_reset_circuit_breaker "Manual reset after investigation"
```

**Disabling Peg:**

```bash
# Edit ubuntu.conf
[peg]
enabled=0

# Restart daemon
./ubud restart
```

## Oracle Integration

### Stub Oracle (Testing)

```ini
[peg]
oracle_type=stub
oracle_config=fixed:1.00        # Fixed price
# or
oracle_config=file:/path/price.txt   # Read from file
# or
oracle_config=random:1.00:0.05  # Random walk
```

### Production Oracles

**File-based:**
```ini
oracle_type=file
oracle_config=/var/lib/ubuntu/price_feed.txt
```

Update price file periodically:
```bash
echo "1.0523" > /var/lib/ubuntu/price_feed.txt
```

**RPC Oracle (future):**
```ini
oracle_type=rpc
oracle_config=http://oracle.example.com/api/price
```

**Aggregated Oracle (future):**
```ini
oracle_type=aggregated
oracle_config=median:source1,source2,source3
```

## Bond Mechanism

When supply needs to contract but the treasury has insufficient funds, bonds are issued:

**Bond Lifecycle:**
1. Contraction needed: 100,000 UBU
2. Treasury has: 60,000 UBU
3. Burn 60,000 UBU from treasury
4. Issue 40,000 UBU in bonds
5. Bonds mature in future epochs
6. Future expansions redeem bonds before treasury

**Bond Configuration:**
```ini
max_bond_debt=1000000000000000  # Maximum total bonds (0 = unlimited)
```

**Querying Bonds:**
```bash
ubu-cli peg_getstatus
# Check "total_bond_debt" field
```

## Testing

### Unit Tests

```bash
# Run unit tests
./tests/unit/peg_controller_tests

# With verbose output
./tests/unit/peg_controller_tests --gtest_filter="*" --gtest_verbose
```

### Integration Tests

```bash
./tests/integration/peg_integration_test
```

### Simulation

```bash
# Spike scenario
./peg_simulator --scenario spike --k 0.05 --epochs 50 --output spike.csv

# Random walk
./peg_simulator --scenario random --k 0.10 --epochs 200 --output random.csv

# Parameter sweep
for k in 0.01 0.05 0.10; do
    ./peg_simulator --scenario random --k $k --epochs 100 --output "k_${k}.csv"
done
```

Analyze results with any CSV tool or Python:
```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('spike.csv')
df.plot(x='epoch', y=['price', 'supply'], secondary_y='supply')
plt.show()
```

## Deterministic Math

All calculations use **integer arithmetic with scaling factors**:

```cpp
// Price scaling: $1.05 = 1,050,000
PRICE_SCALE = 1,000,000

// Coin scaling: 1 UBU = 100,000,000 units
COIN_SCALE = 100,000,000

// PPM scaling: 5% = 50,000 ppm
PPM_SCALE = 1,000,000
```

**Example Calculation:**

```
Price = $1.05 (scaled: 1,050,000)
Supply = 1,000,000 UBU (scaled: 100,000,000,000,000)
k = 0.05 (scaled: 50,000 ppm)

Error = 1,050,000 - 1,000,000 = 50,000

Delta = (k_ppm × error × supply) / (PPM_SCALE × PRICE_SCALE)
      = (50,000 × 50,000 × 100,000,000,000,000) / (1,000,000 × 1,000,000)
      = 250,000,000,000,000 / 1,000,000,000,000
      = 250,000
      = 2,500 UBU
```

This is **fully deterministic** across all platforms.

## Database Schema

**State Persistence:**
```
Key: "peg_state:current"
Value: Serialized PegState struct
```

**Event Log:**
```
Key: "peg_events:<epoch_id>"
Value: Serialized PegEvent struct
```

**Bonds:**
```
Key: "peg_bonds:<bond_id>"
Value: Serialized BondState struct
```

## Performance

**Benchmarks:**
- Epoch execution: < 5ms (typical)
- State persistence: < 1ms
- RPC queries: < 10ms

**Resource Usage:**
- Memory: ~1MB for state
- Disk: ~1KB per epoch event
- CPU: Negligible (runs once per epoch)

## Governance

**Parameter Updates:**

Configuration can be updated via:
1. Edit `ubuntu.conf` and restart (manual)
2. RPC call (hot update): `ubu-cli peg_updateconfig '{"k_ppm": 100000}'`

**Best Practices:**
- Test parameter changes with simulator first
- Update during low-volatility periods
- Monitor for 24 hours after changes
- Keep circuit breaker enabled

## Troubleshooting

**Peg not responding to price changes:**
- Check if enabled: `peg_getstatus`
- Verify oracle freshness: `oracle_max_age_seconds`
- Check if in dead-band range
- Review logs: `grep "PegController" ubuntu.log`

**Circuit breaker keeps triggering:**
- Price volatility too high
- Increase circuit breaker threshold (carefully!)
- Check oracle data quality
- Consider increasing dead-band

**Bonds accumulating:**
- Treasury undersized for volatility
- Increase treasury reserves
- Adjust contraction caps
- Review expansion strategy

## References

- [Original Design Document](../PEG_MODULE_IMPLEMENTATION.md)
- [Unit Tests](../tests/unit/peg_controller_tests.cpp)
- [Integration Tests](../tests/integration/peg_integration_test.cpp)
- [Simulator](../tools/peg_simulator.cpp)

## License

Same as Ubuntu Blockchain (Apache 2.0)
