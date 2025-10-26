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
option(ENABLE_PEG "Enable USD peg mechanism module" ON)

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
    src/consensus/chainparams.cpp

    # Network
    src/network/peer_manager.cpp
    src/network/protocol.cpp
    src/network/network_manager.cpp

    # Storage
    src/storage/database.cpp
    src/storage/block_index.cpp
    src/storage/utxo_db.cpp

    # Mempool
    src/mempool/mempool.cpp
    src/mempool/fee_estimator.cpp

    # Mining
    src/mining/block_assembler.cpp

    # RPC
    src/rpc/rpc_server.cpp
    src/rpc/blockchain_rpc.cpp
    src/rpc/wallet_rpc.cpp

    # Wallet
    src/wallet/wallet.cpp

    # Config
    src/config/config.cpp

    # Metrics
    src/metrics/metrics.cpp
)

# Peg module (conditional)
if(ENABLE_PEG)
    target_sources(ubuntu_core PRIVATE
        src/monetary/peg_controller.cpp
        src/monetary/peg_state_serialization.cpp
        src/monetary/oracle_stub.cpp
        src/monetary/peg_cli.cpp
        src/ledger/ledger_adapter.cpp
    )
    target_compile_definitions(ubuntu_core PUBLIC ENABLE_PEG)
    message(STATUS "Peg module: ENABLED")
else()
    message(STATUS "Peg module: DISABLED")
endif()

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
add_executable(ubud src/daemon/ubud.cpp)
target_link_libraries(ubud PRIVATE ubuntu_core)

# CLI wallet executable
add_executable(ubu-cli src/cli/ubu-cli.cpp)
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

    # Peg module tests (conditional)
    if(ENABLE_PEG)
        add_executable(peg_controller_tests
            tests/unit/peg_controller_tests.cpp
        )
        target_link_libraries(peg_controller_tests PRIVATE
            ubuntu_core
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
        )
        add_test(NAME PegControllerTests COMMAND peg_controller_tests)

        add_executable(peg_integration_test
            tests/integration/peg_integration_test.cpp
        )
        target_link_libraries(peg_integration_test PRIVATE
            ubuntu_core
            GTest::gtest
            GTest::gtest_main
        )
        add_test(NAME PegIntegrationTest COMMAND peg_integration_test)
    endif()
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

# Peg simulator tool (conditional)
if(ENABLE_PEG)
    add_executable(peg_simulator tools/peg_simulator.cpp)
    target_link_libraries(peg_simulator PRIVATE ubuntu_core)
    install(TARGETS peg_simulator RUNTIME DESTINATION bin)
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
message(STATUS "Enable peg module: ${ENABLE_PEG}")
message(STATUS "======================================")
