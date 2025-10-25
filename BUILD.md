# Building Ubuntu Blockchain

This document provides detailed instructions for building the Ubuntu Blockchain from source on various platforms.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Platform-Specific Build Instructions](#platform-specific-build-instructions)
  - [Ubuntu/Debian Linux](#ubuntudebian-linux)
  - [Fedora/RHEL/CentOS](#fedor0@helcentos)
  - [macOS](#macos)
  - [Windows](#windows)
- [Build Options](#build-options)
- [Cross-Compilation](#cross-compilation)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Build Tools

- **CMake** 3.20 or higher
- **C++20 compliant compiler**:
  - GCC 11+ (Linux)
  - Clang 14+ (macOS/Linux)
  - MSVC 2022+ (Windows)
- **Git** (for cloning the repository)

### Required Dependencies

The following libraries are required:

| Library | Minimum Version | Purpose |
|---------|----------------|---------|
| OpenSSL | 3.0+ | Cryptographic operations |
| RocksDB | 7.0+ | Database storage |
| Boost | 1.81+ | System utilities and threading |
| spdlog | 1.10+ | Logging framework |
| nlohmann_json | 3.11+ | JSON processing |
| protobuf | 3.21+ | P2P message serialization |
| secp256k1 | latest | Elliptic curve cryptography |

### Optional Dependencies

| Library | Purpose |
|---------|---------|
| Google Test | Unit testing |
| Google Benchmark | Performance benchmarking |

---

## Platform-Specific Build Instructions

### Ubuntu/Debian Linux

#### 1. Install Build Tools

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config
```

#### 2. Install Dependencies via Package Manager

```bash
sudo apt install -y \
    libssl-dev \
    librocksdb-dev \
    libboost-all-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libsecp256k1-dev
```

#### 3. Install Testing Dependencies (Optional)

```bash
sudo apt install -y \
    libgtest-dev \
    libbenchmark-dev
```

#### 4. Build

```bash
# Clone the repository
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (use -j to parallel build)
cmake --build . -j$(nproc)

# Run tests (optional)
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

#### Using vcpkg (Recommended for Consistent Dependencies)

```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Install dependencies
./vcpkg install \
    openssl \
    rocksdb \
    boost-system \
    boost-thread \
    spdlog \
    nlohmann-json \
    protobuf \
    gtest \
    benchmark

# Build with vcpkg toolchain
cd ../ubuntu-blockchain
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . -j$(nproc)
```

---

### Fedora/RHEL/CentOS

#### 1. Install Build Tools

```bash
# Fedora
sudo dnf install -y gcc-c++ cmake git pkg-config

# RHEL/CentOS (Enable EPEL first)
sudo yum install -y epel-release
sudo yum install -y gcc-c++ cmake3 git pkg-config
```

#### 2. Install Dependencies

```bash
# Fedora
sudo dnf install -y \
    openssl-devel \
    rocksdb-devel \
    boost-devel \
    spdlog-devel \
    json-devel \
    protobuf-devel \
    secp256k1-devel

# RHEL/CentOS
sudo yum install -y \
    openssl-devel \
    boost-devel \
    protobuf-devel

# Note: RocksDB and spdlog may need to be built from source
```

#### 3. Build

```bash
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain
mkdir build && cd build

# Use cmake3 on RHEL/CentOS
cmake .. -DCMAKE_BUILD_TYPE=Release  # or cmake3
cmake --build . -j$(nproc)
```

---

### macOS

#### 1. Install Xcode Command Line Tools

```bash
xcode-select --install
```

#### 2. Install Homebrew (if not already installed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### 3. Install Dependencies

```bash
brew install \
    cmake \
    openssl@3 \
    rocksdb \
    boost \
    spdlog \
    nlohmann-json \
    protobuf \
    secp256k1 \
    googletest \
    google-benchmark
```

#### 4. Build

```bash
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain
mkdir build && cd build

# Configure with Homebrew paths
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)

# Build
cmake --build . -j$(sysctl -n hw.ncpu)

# Run tests
ctest --output-on-failure
```

---

### Windows

#### Using Visual Studio 2022

#### 1. Install Visual Studio 2022

Download and install Visual Studio 2022 with:
- Desktop development with C++
- CMake tools for Windows

#### 2. Install vcpkg

```powershell
# Open PowerShell as Administrator
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Add to PATH (optional)
setx PATH "%PATH%;C:\vcpkg"
```

#### 3. Install Dependencies

```powershell
cd C:\vcpkg
.\vcpkg install `
    openssl:x64-windows `
    rocksdb:x64-windows `
    boost-system:x64-windows `
    boost-thread:x64-windows `
    spdlog:x64-windows `
    nlohmann-json:x64-windows `
    protobuf:x64-windows `
    gtest:x64-windows `
    benchmark:x64-windows
```

#### 4. Build with CMake

```powershell
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain
mkdir build
cd build

cmake .. `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -G "Visual Studio 17 2022" `
    -A x64

cmake --build . --config Release -j
```

#### Using MSYS2/MinGW

```bash
# Install MSYS2 from https://www.msys2.org/

# Update package database
pacman -Syu

# Install build tools
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make git

# Install dependencies
pacman -S \
    mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-rocksdb \
    mingw-w64-x86_64-boost \
    mingw-w64-x86_64-spdlog \
    mingw-w64-x86_64-nlohmann-json \
    mingw-w64-x86_64-protobuf

# Build
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

---

## Build Options

### CMake Configuration Options

```bash
cmake .. [OPTIONS]
```

| Option | Default | Description |
|--------|---------|-------------|
| `-DCMAKE_BUILD_TYPE=<type>` | `Release` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `-DBUILD_TESTS=ON/OFF` | `ON` | Build unit tests |
| `-DBUILD_BENCHMARKS=ON/OFF` | `ON` | Build performance benchmarks |
| `-DENABLE_FUZZING=ON/OFF` | `OFF` | Enable fuzzing support |
| `-DUSE_SANITIZERS=ON/OFF` | `OFF` | Enable address/UB sanitizers |
| `-DCMAKE_INSTALL_PREFIX=<path>` | `/usr/local` | Installation directory |

### Build Type Details

#### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZERS=ON
```
- No optimizations
- Debug symbols included
- Assertions enabled
- Sanitizers available

#### Release Build (Production)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```
- Full optimizations (-O3)
- No debug symbols
- Assertions disabled
- Best performance

#### RelWithDebInfo Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```
- Optimizations enabled
- Debug symbols included
- Good for profiling

### Examples

```bash
# Debug build with tests and sanitizers
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DUSE_SANITIZERS=ON

# Release build without tests
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCHMARKS=OFF

# Custom install location
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local
```

---

## Cross-Compilation

### ARM64 (AArch64) on x86_64

```bash
# Install cross-compiler
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Create toolchain file (arm64-toolchain.cmake)
cat > arm64-toolchain.cmake << EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
EOF

# Build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../arm64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Static Binary

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
cmake --build . -j$(nproc)
```

---

## Running Tests

```bash
# From build directory

# Run all tests
ctest --output-on-failure

# Run specific test
ctest -R TransactionTest --verbose

# Run tests in parallel
ctest -j$(nproc)

# Run benchmarks
./bin/ubu_bench

# Run specific benchmark
./bin/ubu_bench --benchmark_filter=BM_SHA256
```

---

## Installation

```bash
# From build directory

# Install to system (requires sudo)
sudo cmake --install .

# Install to custom location
cmake --install . --prefix $HOME/.local

# Create package
cpack -G TGZ  # Create .tar.gz
cpack -G DEB  # Create .deb (Debian/Ubuntu)
cpack -G RPM  # Create .rpm (Fedora/RHEL)
```

---

## Performance Optimization

### Compiler Optimizations

```bash
# Maximum optimization
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"

# Link-Time Optimization (LTO)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### Profile-Guided Optimization (PGO)

```bash
# 1. Build with profiling
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-fprofile-generate"
cmake --build . -j$(nproc)

# 2. Run typical workload
./bin/ubud -regtest
# ... perform typical operations ...
# ... then stop daemon ...

# 3. Rebuild with profile data
cmake .. \
    -DCMAKE_CXX_FLAGS="-fprofile-use"
cmake --build . -j$(nproc)
```

---

## Troubleshooting

### Common Issues

#### 1. CMake Can't Find Dependencies

**Solution**: Use vcpkg or specify paths manually:

```bash
cmake .. \
    -DCMAKE_PREFIX_PATH="/path/to/deps" \
    -DBoost_ROOT="/path/to/boost" \
    -DOPENSSL_ROOT_DIR="/path/to/openssl"
```

#### 2. OpenSSL Version Mismatch

**Solution**: Ensure OpenSSL 3.0+ is installed:

```bash
# Check version
openssl version

# Ubuntu: Install from PPA if needed
sudo add-apt-repository ppa:ondrej/openssl
sudo apt update
sudo apt install openssl libssl-dev
```

#### 3. RocksDB Not Found

**Solution**: Build and install RocksDB from source:

```bash
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=OFF
cmake --build . -j$(nproc)
sudo cmake --install .
```

#### 4. secp256k1 Not Found

**Solution**: Build from Bitcoin Core repository:

```bash
git clone https://github.com/bitcoin-core/secp256k1.git
cd secp256k1
./autogen.sh
./configure --enable-module-recovery
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### 5. Linking Errors on Linux

**Solution**: Update library cache:

```bash
sudo ldconfig
```

#### 6. C++20 Features Not Available

**Solution**: Update compiler:

```bash
# Ubuntu
sudo apt install gcc-11 g++-11
export CC=gcc-11
export CXX=g++-11

# Or use Clang
sudo apt install clang-14
export CC=clang-14
export CXX=clang++-14
```

### Build Logs

Enable verbose output for debugging:

```bash
# Verbose CMake
cmake .. --trace

# Verbose build
cmake --build . --verbose

# Or with make
make VERBOSE=1
```

---

## Docker Build

For containerized builds, see [DEPLOYMENT.md](docs/DEPLOYMENT.md#docker-deployment).

```bash
# Build Docker image
docker build -t ubuntu-blockchain:latest .

# Run container
docker run -d --name ubud \
    -p 8333:8333 \
    -p 8332:8332 \
    -v blockchain-data:/var/lib/ubuntu-blockchain \
    ubuntu-blockchain:latest
```

---

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-13, windows-2022]
        build_type: [Debug, Release]

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies (Ubuntu)
      if: runner.os == 'Linux'
      run: |
        sudo apt update
        sudo apt install -y libssl-dev librocksdb-dev libboost-all-dev

    - name: Install dependencies (macOS)
      if: runner.os == 'macOS'
      run: brew install openssl rocksdb boost spdlog

    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

    - name: Build
      run: cmake --build build -j

    - name: Test
      run: cd build && ctest --output-on-failure
```

---

## Additional Resources

- [README.md](README.md) - Project overview and quick start
- [DEPLOYMENT.md](docs/DEPLOYMENT.md) - Deployment and configuration guide
- [API.md](docs/API.md) - RPC API reference
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines

For build-related questions, please open an issue on GitHub: https://github.com/EmekaIwuagwu/ubuntu-blockchain/issues
