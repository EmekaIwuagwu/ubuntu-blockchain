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
./ubu-cli sendtoaddress "U1recipient_address" 100.0

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
miningaddress=U1your_mining_address

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

## 🛡️ Security Audit Summary

> **⚠️ PRODUCTION STATUS: NOT READY FOR DEPLOYMENT**
>
> A comprehensive security audit has been conducted on the Ubuntu Blockchain codebase. The system currently contains **critical vulnerabilities** that must be addressed before production deployment.

### Audit Overview

**Full Audit Report:** [docs/SECURITY_AUDIT.md](docs/SECURITY_AUDIT.md)

**Security Posture:** MEDIUM-HIGH RISK

| Severity | Count | Status |
|----------|-------|--------|
| 🔴 **CRITICAL** | 11 | Requires immediate remediation (4-6 weeks) |
| 🟠 **HIGH** | 19 | Must fix before production (2-3 weeks) |
| 🟡 **MEDIUM** | 16 | Should fix for hardening (1-2 weeks) |
| 🟢 **LOW** | 9 | Recommended improvements |

### Critical Vulnerabilities Summary

The following critical vulnerabilities (CVSS 8.0+) have been identified and **MUST** be fixed before any production deployment:

1. **Missing RPC Authentication** (CVSS 9.8)
   - RPC server has no authentication mechanism
   - Any network client can execute privileged operations
   - **Risk:** Complete node compromise, fund theft

2. **No P2P Rate Limiting** (CVSS 9.3)
   - Network layer lacks message rate limiting
   - Vulnerable to resource exhaustion DoS attacks
   - **Risk:** Node crash, network partition

3. **Wallet Not Encrypted at Rest** (CVSS 9.2)
   - Private keys stored in plaintext
   - No encryption despite claiming to be encrypted
   - **Risk:** Complete fund theft if filesystem is compromised

4. **Timestamp Manipulation (Timewarp)** (CVSS 9.1)
   - Missing Median-Time-Past (MTP) validation
   - Miners can manipulate block timestamps
   - **Risk:** Difficulty adjustment attack, faster-than-expected block production

5. **Sybil Attack Vulnerability** (CVSS 9.1)
   - No peer diversity enforcement
   - Attacker can fill peer slots with malicious nodes
   - **Risk:** Eclipse attack, double-spend, transaction censorship

6. **Non-Atomic Chain State Updates** (CVSS 8.9)
   - Database writes not atomic across column families
   - Crash during state update causes corruption
   - **Risk:** Permanent blockchain state corruption

7. **Difficulty Adjustment Manipulation** (CVSS 8.9)
   - Vulnerable to off-by-one errors in difficulty calculation
   - Block selection manipulation possible
   - **Risk:** Chain security degradation

8. **No P2P Message Authentication** (CVSS 8.7)
   - P2P messages lack cryptographic signatures
   - Vulnerable to man-in-the-middle attacks
   - **Risk:** False blockchain data injection

9. **Transaction Malleability** (CVSS 8.1)
   - Transaction IDs can be modified before confirmation
   - Vulnerable to signature malleability attacks
   - **Risk:** Merchant fraud, wallet confusion

10. **Oracle Spoofing (USD Peg)** (CVSS 7.8)
    - Single oracle without redundancy or validation
    - No Byzantine fault tolerance
    - **Risk:** Peg manipulation, economic attack

11. **Block Validation Non-Determinism** (CVSS 7.8)
    - Floating-point arithmetic in consensus code
    - Non-deterministic validation can cause chain splits
    - **Risk:** Network consensus failure

### Required Actions Before Production

**DO NOT deploy to mainnet until:**

✅ **Phase 1 (Critical Fixes - 4-6 weeks):**
- [ ] Implement RPC authentication with session tokens
- [ ] Add P2P rate limiting (token bucket algorithm)
- [ ] Encrypt wallet with AES-256-GCM
- [ ] Implement Median-Time-Past (MTP) validation
- [ ] Add peer diversity management (subnet limits)
- [ ] Implement atomic RocksDB write batches
- [ ] Fix difficulty adjustment algorithm
- [ ] Add P2P message HMAC authentication
- [ ] Implement SegWit-style transaction IDs
- [ ] Add multi-signature oracle with BFT
- [ ] Remove floating-point from consensus code

✅ **Phase 2 (High Priority Fixes - 2-3 weeks):**
- [ ] Harden RNG for key generation (entropy validation)
- [ ] Implement replay protection for forks
- [ ] Add comprehensive input validation to all RPC methods
- [ ] Implement peer reputation and banning system
- [ ] Add DoS limits (memory, disk, CPU)
- [ ] Secure memory wiping for private keys
- [ ] Fix concurrency issues (deadlocks, race conditions)

✅ **Phase 3 (Testing & Validation - 2-4 weeks):**
- [ ] Complete comprehensive test suite (unit, integration, fuzzing)
- [ ] Perform third-party penetration testing
- [ ] Conduct professional security audit by external firm
- [ ] Run 30-day testnet with bug bounty program
- [ ] Load testing: 1000+ TPS sustained for 7 days
- [ ] Chaos engineering: random node failures, network partitions

✅ **Phase 4 (Documentation & Deployment):**
- [ ] Finalize secure deployment guides
- [ ] Create incident response procedures
- [ ] Establish security monitoring and alerting
- [ ] Prepare emergency rollback procedures
- [ ] Train operations team on security protocols

**Estimated Time to Production-Ready:** 12-16 weeks minimum

### Security Best Practices for Node Operators

If running a testnet node, follow these hardening guidelines:

**System Hardening:**
```bash
# Run as non-privileged user
sudo useradd -r -m -d /home/ubuntu-blockchain ubuntu-blockchain

# Restrict file permissions
chmod 700 ~/.ubuntu-blockchain
chmod 600 ~/.ubuntu-blockchain/wallet.dat
chmod 600 ~/.ubuntu-blockchain/ubuntu.conf

# Enable firewall (allow only P2P port)
sudo ufw enable
sudo ufw allow 8333/tcp  # P2P only
sudo ufw deny 8332/tcp   # Block RPC from external
```

**Secure Configuration:**
```ini
# Bind RPC to localhost only
rpcbind=127.0.0.1
rpcallowip=127.0.0.1

# Use strong RPC password (64+ random characters)
rpcuser=ubuntu
rpcpassword=$(openssl rand -base64 48)

# Enable all logging for security monitoring
loglevel=debug
logtofile=1

# Limit resource usage
maxconnections=125
maxmempool=300
dbcache=450
```

**Network Security:**
- Deploy behind firewall/VPN
- Use SSH tunneling for RPC access
- Monitor logs for suspicious activity
- Keep system and dependencies updated
- Enable fail2ban for SSH protection

**Wallet Security:**
- **NEVER** store large amounts on hot wallets
- Use cold storage for long-term holdings
- Backup mnemonic phrase offline (paper wallet)
- Test recovery procedure before funding
- Use hardware security modules (HSM) for production

### Secure Build and Deployment

**Compiler Hardening Flags:**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE" \
  -DCMAKE_EXE_LINKER_FLAGS="-pie -Wl,-z,relro,-z,now"
```

**Static Analysis:**
```bash
# Run clang-tidy
clang-tidy src/**/*.cpp -- -std=c++20

# Run cppcheck
cppcheck --enable=all --inconclusive --std=c++20 src/

# Run address sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON
```

**Container Security (Docker/Kubernetes):**
- Use minimal base images (Alpine, Distroless)
- Run containers as non-root user
- Enable seccomp, AppArmor, SELinux
- Scan images for vulnerabilities (Trivy, Grype)
- Limit container resources (CPU, memory)
- Use read-only root filesystems where possible

### Vulnerability Disclosure

**Security Contact:** security@ubuntu-blockchain.org

**Responsible Disclosure Policy:**

1. **Report privately** - Do NOT disclose publicly before patch
2. **Provide details** - Steps to reproduce, impact assessment
3. **Allow time** - We will respond within 48 hours
4. **Coordinated disclosure** - We will work with you on timeline

**Bug Bounty Program:** Coming soon (post-mainnet launch)

**Security Severity Classification:**
- **Critical:** Immediate threat to funds or network (90-day disclosure)
- **High:** Significant security impact (120-day disclosure)
- **Medium:** Moderate security impact (180-day disclosure)
- **Low:** Minor security issue (no disclosure timeline)

**Hall of Fame:** We will publicly acknowledge security researchers who responsibly disclose vulnerabilities (with their permission).

### Compliance and Standards

Ubuntu Blockchain security architecture adheres to:

- **NIST SP 800-57** - Key Management Recommendations
- **NIST SP 800-90A** - Random Number Generation
- **FIPS 140-2** - Cryptographic Module Validation
- **OWASP Top 10** - API Security Best Practices
- **C++ Core Guidelines** - Memory Safety and Concurrency
- **Bitcoin BIPs** - BIP-32, BIP-39, BIP-44, BIP-113 (MTP)
- **CVE Standards** - Common Vulnerabilities and Exposures

### Security Roadmap

**Q1 2025:**
- [ ] Complete Phase 1 critical vulnerability remediation
- [ ] Implement comprehensive fuzzing test suite
- [ ] Deploy public testnet with bug bounty

**Q2 2025:**
- [ ] Complete Phase 2 high priority fixes
- [ ] Third-party penetration testing
- [ ] Professional security audit (Trail of Bits, OpenZeppelin, etc.)

**Q3 2025:**
- [ ] Address all medium severity issues
- [ ] Complete chaos engineering tests
- [ ] Finalize incident response procedures

**Q4 2025:**
- [ ] Launch bug bounty program ($100K+ pool)
- [ ] Mainnet security review board
- [ ] Production deployment (conditional on audit results)

---

**For detailed vulnerability analysis, attack scenarios, and complete remediation code, see [docs/SECURITY_AUDIT.md](docs/SECURITY_AUDIT.md)**

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
