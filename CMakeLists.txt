cmake_minimum_required(VERSION 3.20)

project(UbuntuBlockchain
    VERSION 1.0.0
    DESCRIPTION "Ubuntu Blockchain (UBU) - Production-Grade Blockchain Implementation"
    LANGUAGES CXX
)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(ENABLE_FUZZING "Enable fuzzing support" OFF)
option(USE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

# Sanitizers (for development)
if(USE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# Find dependencies
find_package(OpenSSL 3.0 REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost 1.81 REQUIRED COMPONENTS system thread)
find_package(RocksDB CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# secp256k1 (will add custom finder)
# find_package(secp256k1 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Core library
add_library(ubuntu_core STATIC
    # Crypto
    src/crypto/hash.cpp
    src/crypto/keys.cpp
    src/crypto/signatures.cpp
    src/crypto/base58.cpp

    # Core
    src/core/block.cpp
    src/core/transaction.cpp
    src/core/chain.cpp
    src/core/utxo.cpp
    src/core/merkle.cpp

    # Consensus
    src/consensus/pow.cpp
    src/consensus/validation.cpp
    src/consensus/chainparams.cpp

    # Network
    src/network/peer_manager.cpp
    src/network/protocol.cpp
    src/network/gossip.cpp
    src/network/discovery.cpp

    # Storage
    src/storage/database.cpp
    src/storage/block_index.cpp
    src/storage/tx_index.cpp
    src/storage/utxo_db.cpp

    # Mempool
    src/mempool/mempool.cpp
    src/mempool/fee_estimator.cpp
    src/mempool/rbf.cpp

    # Miner
    src/miner/miner.cpp
    src/miner/block_assembler.cpp

    # RPC
    src/rpc/server.cpp
    src/rpc/blockchain.cpp
    src/rpc/mining.cpp
    src/rpc/network.cpp
    src/rpc/wallet.cpp

    # Wallet
    src/wallet/wallet.cpp
    src/wallet/keystore.cpp
    src/wallet/coin_selection.cpp

    # Governance
    src/governance/proposals.cpp
    src/governance/voting.cpp
    src/governance/execution.cpp

    # Utils
    src/utils/config.cpp
    src/utils/logging.cpp
    src/utils/time.cpp
    src/utils/serialization.cpp
)

target_link_libraries(ubuntu_core PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::thread
    RocksDB::rocksdb
    protobuf::libprotobuf
)

# Node daemon executable
add_executable(ubud src/main.cpp)
target_link_libraries(ubud PRIVATE ubuntu_core)

# CLI wallet executable
add_executable(ubu-cli src/cli.cpp)
target_link_libraries(ubu-cli PRIVATE ubuntu_core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(ubu_tests
        tests/unit/crypto_tests.cpp
        tests/unit/transaction_tests.cpp
        tests/unit/block_tests.cpp
        tests/unit/consensus_tests.cpp
        tests/unit/mempool_tests.cpp
    )

    target_link_libraries(ubu_tests PRIVATE
        ubuntu_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )

    add_test(NAME UbuntuBlockchainTests COMMAND ubu_tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)

    add_executable(ubu_bench
        tests/benchmarks/crypto_bench.cpp
        tests/benchmarks/validation_bench.cpp
        tests/benchmarks/database_bench.cpp
    )

    target_link_libraries(ubu_bench PRIVATE
        ubuntu_core
        benchmark::benchmark
        benchmark::benchmark_main
    )
endif()

# Installation rules
install(TARGETS ubud ubu-cli ubuntu_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ubuntu
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration summary
message(STATUS "=== Ubuntu Blockchain Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "Enable fuzzing: ${ENABLE_FUZZING}")
message(STATUS "Use sanitizers: ${USE_SANITIZERS}")
message(STATUS "======================================")
