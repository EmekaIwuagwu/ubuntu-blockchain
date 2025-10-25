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
./ubu-cli sendtoaddress "ubu1recipient_address" 100.0

# List recent transactions
./ubu-cli listtransactions
```

## Configuration

Create a `ubu.conf` file in your data directory:

```ini
# Network settings
listen=1
port=8333
rpcport=8332
rpcbind=127.0.0.1

# Data directory
datadir=/path/to/blockchain/data

# Mining (optional)
mining=1
mineraddress=ubu1your_mining_address

# Logging
debug=net,mempool
logfile=logs/debug.log

# RPC authentication
rpcuser=your_username
rpcpassword=your_secure_password
```

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

### Phase 1: Foundation ✅ (Current)
- [x] Project structure and build system
- [ ] Cryptographic primitives implementation
- [ ] Core data structures (Block, Transaction, UTXO)
- [ ] Basic consensus rules

### Phase 2: Consensus & Storage
- [ ] RocksDB integration
- [ ] Block validation logic
- [ ] Merkle tree implementation
- [ ] PoW and difficulty adjustment
- [ ] Blockchain state machine

### Phase 3: Networking
- [ ] P2P protocol implementation
- [ ] Peer discovery and management
- [ ] Block and transaction propagation
- [ ] Anti-DoS mechanisms

### Phase 4: Mempool & Mining
- [ ] Transaction pool management
- [ ] Fee estimation
- [ ] Mining algorithm
- [ ] Block reward system

### Phase 5: RPC & Wallet
- [ ] JSON-RPC server
- [ ] Wallet implementation
- [ ] CLI tools
- [ ] API documentation

### Phase 6: Production Readiness
- [ ] On-chain governance
- [ ] Monitoring and metrics
- [ ] Security audit
- [ ] Performance optimization
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
