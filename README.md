# Ubuntu Blockchain (UBU)

**Production-Grade Blockchain Implementation in Modern C++**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

## Overview

Ubuntu Blockchain (UBU) is a military-grade, production-ready blockchain system designed to support sovereign financial infrastructure. Built from the ground up using modern C++ (C++20/C++23), UBU is engineered to process billions of transactions with absolute reliability, security, and performance.

### Key Features

- **Native Currency:** Ubuntu Blockchain Token (UBU)
- **Initial Supply:** 1,000,000,000,000 UBU (1 trillion)
- **Peg Value:** 1 UBU = 1 USD
- **Consensus:** Proof of Work (PoW) with intelligent difficulty adjustment
- **Transaction Model:** UTXO-based (similar to Bitcoin)
- **Target Performance:** >1 billion transactions with sub-second query times
- **Security:** Banking-grade cryptographic primitives
- **Scalability:** Designed for national-scale financial systems

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         RPC LAYER                            │
│  (JSON-RPC Server, REST API, CLI Wallet Interface)          │
└────────────┬────────────────────────────────────────────────┘
             │
┌────────────▼────────────────────────────────────────────────┐
│                      NODE CORE ENGINE                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Mempool    │  │    Miner     │  │  Governance  │      │
│  │   Manager    │  │   (PoW)      │  │   System     │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │               │
│  ┌──────▼──────────────────▼──────────────────▼───────┐     │
│  │         BLOCKCHAIN STATE MACHINE                    │     │
│  │  (Block Validation, Chain Selection, UTXO Set)      │     │
│  └──────┬──────────────────────────────────────────────┘     │
└─────────┼──────────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────────┐
│                    STORAGE LAYER (RocksDB)                   │
│  [Blocks] [UTXO Index] [Tx Index] [Chain State] [Peers]     │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                    P2P NETWORK LAYER                         │
│  (Peer Discovery, Gossip Protocol, Message Validation)      │
└──────────────────────────────────────────────────────────────┘
```

## Technology Stack

- **Language:** C++20/C++23
- **Build System:** CMake 3.20+
- **Cryptography:** libsecp256k1, OpenSSL 3.0+
- **Database:** RocksDB 7.0+
- **Networking:** Boost.Asio 1.81+
- **Serialization:** Protocol Buffers 3.21+
- **Logging:** spdlog 1.11+
- **Testing:** GoogleTest, Google Benchmark
- **JSON:** nlohmann/json 3.11+

## Building from Source

### Prerequisites

- Visual Studio 2022 (Windows) or GCC 11+/Clang 14+ (Linux/macOS)
- CMake 3.20 or higher
- vcpkg (for dependency management)
- Git

### Windows (Visual Studio 2022)

```powershell
# Clone the repository
git clone https://github.com/UbuntuBlockchain/ubuntu-blockchain.git
cd ubuntu-blockchain

# Install dependencies with vcpkg
vcpkg install

# Configure and build
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Run tests
cd build
ctest -C Release
```

### Linux/macOS

```bash
# Clone the repository
git clone https://github.com/UbuntuBlockchain/ubuntu-blockchain.git
cd ubuntu-blockchain

# Install dependencies with vcpkg
vcpkg install

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# Run tests
cd build
ctest
```

## Quick Start

### Running a Full Node

```bash
# Initialize the data directory
./ubud --datadir=/path/to/data --init

# Start the node
./ubud --datadir=/path/to/data --daemon

# Check node status
./ubu-cli getblockchaininfo
```

### Using the Wallet

```bash
# Generate a new address
./ubu-cli getnewaddress "my-wallet"

# Check balance
./ubu-cli getbalance

# Send UBU
./ubu-cli sendtoaddress "u1recipient_address" 100.0

# List recent transactions
./ubu-cli listtransactions
```

## Configuration

Create a `ubuntu.conf` file in your data directory (`~/.ubuntu-blockchain/ubuntu.conf`):

```ini
# Network settings
port=8333
maxconnections=125

# RPC Server settings
server=1
rpcport=8332
rpcbind=127.0.0.1
rpcuser=ubuntu
rpcpassword=changeme_secure_password

# Data directory
datadir=~/.ubuntu-blockchain

# Database cache (MB)
dbcache=450

# Mining (optional)
mining=0
miningthreads=0
miningaddress=u1your_mining_address

# Mempool
maxmempool=300
minrelaytxfee=1

# Logging
loglevel=info
logtofile=1
logfile=~/.ubuntu-blockchain/debug.log

# Metrics (Prometheus compatible)
metrics=1
metricsport=9090
```

See `ubuntu.conf.example` for all available options.

## Project Structure

```
ubuntu-blockchain/
├── cmake/               # CMake modules
├── include/ubuntu/      # Public headers
│   ├── crypto/         # Cryptographic primitives
│   ├── core/           # Block, transaction structures
│   ├── consensus/      # Consensus algorithms
│   ├── network/        # P2P networking
│   └── utils/          # Utilities
├── src/                # Implementation files
├── tests/              # Test suites
│   ├── unit/          # Unit tests
│   ├── integration/   # Integration tests
│   ├── functional/    # Functional tests
│   ├── fuzz/          # Fuzzing tests
│   └── benchmarks/    # Performance benchmarks
├── docs/              # Documentation
└── scripts/           # Build and utility scripts
```

## Development Roadmap

### Phase 1: Foundation ✅ **COMPLETE**
- [x] Project structure and build system
- [x] Cryptographic primitives (SHA-256, RIPEMD-160, secp256k1)
- [x] Core data structures (Block, Transaction, UTXO)
- [x] BIP-32/39/44 HD wallet implementation
- [x] Base58Check and Bech32 address encoding

### Phase 2: Consensus & Storage ✅ **COMPLETE**
- [x] RocksDB integration with column families
- [x] Block validation logic with full consensus rules
- [x] Merkle tree with SPV proof support
- [x] PoW mining and difficulty adjustment (Bitcoin-compatible)
- [x] Blockchain state machine with reorganization

### Phase 3: Networking ✅ **COMPLETE**
- [x] P2P protocol implementation (Protocol Buffers)
- [x] Peer discovery and connection management
- [x] Block and transaction propagation (INV/GETDATA/BLOCK)
- [x] Ban scoring and misbehavior detection
- [x] Header-first synchronization

### Phase 4: Mempool & Mining ✅ **COMPLETE**
- [x] Transaction pool with priority ordering
- [x] Fee estimation based on block history
- [x] Replace-By-Fee (RBF) support
- [x] Block assembly with optimal transaction selection
- [x] Dependency tracking and conflict detection

### Phase 5: RPC & Wallet ✅ **COMPLETE**
- [x] JSON-RPC 2.0 server (31 methods)
- [x] HD wallet with secure key management
- [x] CLI tools (ubu-cli)
- [x] Transaction creation and signing
- [x] Blockchain and wallet RPC methods

### Phase 6: Production Features ⏳ **IN PROGRESS**
- [x] Configuration file system (ubuntu.conf)
- [x] Monitoring and metrics (Prometheus/JSON)
- [x] Performance optimizations
- [x] Security hardening
- [ ] Comprehensive testing suite
- [ ] Deployment documentation
- [ ] Mainnet launch preparation

## Performance Targets

| Metric | Target |
|--------|--------|
| Transactions/sec (sustained) | 1000+ TPS |
| Block validation time | <100ms |
| UTXO lookup time | <1ms |
| Signature verification | 10,000/sec |
| Initial sync time | <2 hours (1M blocks) |
| Memory usage | <4GB (full node) |

## Security

Ubuntu Blockchain employs military-grade security measures:

- **Cryptography:** Secp256k1 ECDSA signatures, SHA-256d hashing
- **Memory Safety:** Modern C++ with RAII, smart pointers
- **Input Validation:** Comprehensive validation on all external data
- **Network Security:** Rate limiting, ban system, encrypted RPC
- **Audit Trail:** Immutable logging of all state changes

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run tests (`ctest`)
5. Format code (`clang-format -i **/*.cpp **/*.h`)
6. Commit changes (`git commit -m 'Add amazing feature'`)
7. Push to branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

## Testing

```bash
# Run all tests
ctest

# Run specific test suite
./build/bin/ubu_tests --gtest_filter=CryptoTests.*

# Run benchmarks
./build/bin/ubu_bench

# Run functional tests
python tests/functional/run_tests.py
```

## RPC Methods

Ubuntu Blockchain provides 31 JSON-RPC 2.0 methods:

### Blockchain Methods (18)
- `getblockchaininfo` - Get blockchain status and statistics
- `getblockcount` - Get current block height
- `getbestblockhash` - Get hash of the best block
- `getdifficulty` - Get current mining difficulty
- `getchaintips` - Get information about chain tips
- `getblock <hash>` - Get block by hash
- `getblockhash <height>` - Get block hash by height
- `getblockheader <hash>` - Get block header information
- `getrawtransaction <txid>` - Get transaction by hash
- `decoderawtransaction <hex>` - Decode raw transaction
- `sendrawtransaction <hex>` - Broadcast transaction
- `getmempoolinfo` - Get mempool statistics
- `getrawmempool` - List mempool transactions
- `getmempoolentry <txid>` - Get mempool transaction details
- `getconnectioncount` - Get number of peers
- `getpeerinfo` - Get detailed peer information
- `getnetworkinfo` - Get network statistics
- `validateaddress <address>` - Validate address format
- `estimatefee <nblocks>` - Estimate fee for confirmation

### Wallet Methods (13)
- `getwalletinfo` - Get wallet status and balance
- `getbalance` - Get confirmed balance
- `getunconfirmedbalance` - Get unconfirmed balance
- `getnewaddress [label]` - Generate new address
- `listaddresses` - List all wallet addresses
- `getaddressesbylabel <label>` - Get addresses by label
- `sendtoaddress <address> <amount>` - Send to address
- `sendmany <recipients>` - Send to multiple recipients
- `listtransactions [count] [skip]` - List transactions
- `gettransaction <txid>` - Get transaction details
- `dumpprivkey <address>` - Export private key
- `importprivkey <wif>` - Import private key
- `backupwallet <destination>` - Backup wallet file

Use `ubu-cli help <method>` for detailed usage.

## Documentation

- [Architecture Overview](docs/architecture.md)
- [Protocol Specification](docs/protocol.md)
- [RPC API Reference](docs/rpc-api.md)
- [Wallet Guide](docs/wallet-guide.md)
- [Mining Guide](docs/mining-guide.md)
- [Deployment Guide](docs/deployment.md)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Bitcoin Core for pioneering blockchain architecture
- Ethereum for smart contract innovations
- RocksDB team for the excellent storage engine
- The entire blockchain open-source community

## Contact

- Website: https://ubuntu-blockchain.org
- Email: dev@ubuntu-blockchain.org
- Discord: https://discord.gg/ubuntu-blockchain
- Twitter: @UbuntuBlockchain

---

**Built with precision. Secured by design. Scaled for the future.**

⛓️ Ubuntu Blockchain - Financial Freedom for All
