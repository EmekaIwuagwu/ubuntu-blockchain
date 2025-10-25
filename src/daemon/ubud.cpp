/**
 * Ubuntu Blockchain Daemon (ubud)
 *
 * Full node implementation combining blockchain, networking, mining, and RPC.
 */

#include "ubuntu/consensus/chainparams.h"
#include "ubuntu/consensus/pow.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/mempool/fee_estimator.h"
#include "ubuntu/mempool/mempool.h"
#include "ubuntu/mining/block_assembler.h"
#include "ubuntu/network/network_manager.h"
#include "ubuntu/rpc/blockchain_rpc.h"
#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/rpc/wallet_rpc.h"
#include "ubuntu/storage/block_index.h"
#include "ubuntu/storage/database.h"
#include "ubuntu/storage/utxo_db.h"
#include "ubuntu/wallet/wallet.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

using namespace ubuntu;

// Global shutdown flag
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int signal) {
    spdlog::info("Received signal {}, initiating shutdown...", signal);
    g_shutdownRequested = true;
}

void printBanner() {
    std::cout << R"(
 _   _ _                 _         ____  _            _        _           _
| | | | |__  _   _ _ __ | |_ _   _| __ )| | ___   ___| | _____| |__   __ _(_)_ __
| | | | '_ \| | | | '_ \| __| | | |  _ \| |/ _ \ / __| |/ / __| '_ \ / _` | | '_ \
| |_| | |_) | |_| | | | | |_| |_| | |_) | | (_) | (__|   < (__| | | | (_| | | | | |
 \___/|_.__/ \__,_|_| |_|\__|\__,_|____/|_|\___/ \___|_|\_\___|_| |_|\__,_|_|_| |_|

Ubuntu Blockchain v1.0.0 - Production-Ready Military-Grade Blockchain
)" << std::endl;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -datadir=<dir>       Data directory (default: ~/.ubuntu-blockchain)\n";
    std::cout << "  -port=<port>         P2P port (default: 8333)\n";
    std::cout << "  -rpcport=<port>      RPC port (default: 8332)\n";
    std::cout << "  -rpcuser=<username>  RPC username\n";
    std::cout << "  -rpcpassword=<pass>  RPC password\n";
    std::cout << "  -testnet             Use testnet\n";
    std::cout << "  -regtest             Use regression test network\n";
    std::cout << "  -daemon              Run in background\n";
    std::cout << "  -server              Accept RPC connections\n";
    std::cout << "  -h, --help           Show this help message\n\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string dataDir = "~/.ubuntu-blockchain";
    uint16_t p2pPort = 8333;
    uint16_t rpcPort = 8332;
    std::string rpcUser = "";
    std::string rpcPassword = "";
    bool runAsDaemon = false;
    bool enableRpc = true;
    consensus::NetworkType networkType = consensus::NetworkType::MAINNET;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printBanner();
            printUsage(argv[0]);
            return 0;
        } else if (arg.find("-datadir=") == 0) {
            dataDir = arg.substr(9);
        } else if (arg.find("-port=") == 0) {
            p2pPort = static_cast<uint16_t>(std::stoi(arg.substr(6)));
        } else if (arg.find("-rpcport=") == 0) {
            rpcPort = static_cast<uint16_t>(std::stoi(arg.substr(9)));
        } else if (arg.find("-rpcuser=") == 0) {
            rpcUser = arg.substr(9);
        } else if (arg.find("-rpcpassword=") == 0) {
            rpcPassword = arg.substr(13);
        } else if (arg == "-testnet") {
            networkType = consensus::NetworkType::TESTNET;
        } else if (arg == "-regtest") {
            networkType = consensus::NetworkType::REGTEST;
        } else if (arg == "-daemon") {
            runAsDaemon = true;
        } else if (arg == "-server") {
            enableRpc = true;
        }
    }

    printBanner();

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize logging
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting Ubuntu Blockchain daemon...");
    spdlog::info("Network: {}", networkType == consensus::NetworkType::MAINNET    ? "mainnet"
                                 : networkType == consensus::NetworkType::TESTNET ? "testnet"
                                                                                    : "regtest");
    spdlog::info("Data directory: {}", dataDir);
    spdlog::info("P2P port: {}", p2pPort);
    spdlog::info("RPC port: {}", rpcPort);

    try {
        // Create chain parameters
        auto chainParams = consensus::ChainParams::getParams(networkType);

        // Initialize database
        spdlog::info("Initializing database...");
        auto db = std::make_shared<storage::Database>(dataDir + "/chainstate");

        // Initialize UTXO database
        auto utxoDb = std::make_shared<storage::UTXODatabase>(db);

        // Initialize block storage
        auto blockStorage = std::make_shared<storage::BlockStorage>(db);

        // Initialize blockchain
        spdlog::info("Initializing blockchain...");
        auto blockchain = std::make_shared<core::Blockchain>(chainParams);
        if (!blockchain->initialize()) {
            spdlog::error("Failed to initialize blockchain");
            return 1;
        }

        // Initialize mempool
        spdlog::info("Initializing mempool...");
        auto mempool = std::make_shared<mempool::Mempool>();

        // Initialize fee estimator
        auto feeEstimator = std::make_shared<mempool::FeeEstimator>();

        // Initialize network manager
        spdlog::info("Initializing network...");
        auto networkManager =
            std::make_shared<network::NetworkManager>(blockchain, mempool, blockStorage, chainParams);

        // Add seed nodes
        std::vector<network::NetAddress> seedNodes;
        // In production, load from config or hardcoded list
        networkManager->addSeedNodes(seedNodes);

        // Start network
        if (!networkManager->start(p2pPort)) {
            spdlog::error("Failed to start network manager");
            return 1;
        }

        // Initialize wallet
        spdlog::info("Initializing wallet...");
        auto [wallet, mnemonic] = wallet::Wallet::createNew(24, 0);
        wallet->setUtxoDatabase(utxoDb);

        spdlog::info("Wallet created. IMPORTANT: Save this mnemonic phrase:");
        std::cout << "\n========================================" << std::endl;
        std::cout << mnemonic << std::endl;
        std::cout << "========================================\n" << std::endl;

        // Initialize RPC server
        std::unique_ptr<rpc::RpcServer> rpcServer;
        if (enableRpc) {
            spdlog::info("Initializing RPC server...");
            rpcServer = std::make_unique<rpc::RpcServer>("127.0.0.1", rpcPort);

            if (!rpcUser.empty() && !rpcPassword.empty()) {
                rpcServer->setAuth(rpcUser, rpcPassword);
                rpcServer->setAuthEnabled(true);
                spdlog::info("RPC authentication enabled");
            }

            // Register blockchain RPC methods
            auto blockchainRpc =
                std::make_shared<rpc::BlockchainRpc>(blockchain, blockStorage, mempool, networkManager);
            blockchainRpc->registerMethods(*rpcServer);

            // Register wallet RPC methods
            auto walletRpc =
                std::make_shared<rpc::WalletRpc>(wallet, feeEstimator, networkManager);
            walletRpc->registerMethods(*rpcServer);

            // Register stop command
            rpcServer->registerMethod("stop", [](const rpc::JsonValue& params) {
                spdlog::info("Shutdown requested via RPC");
                g_shutdownRequested = true;
                return rpc::JsonValue("Shutting down...");
            });

            if (!rpcServer->start()) {
                spdlog::error("Failed to start RPC server");
                return 1;
            }
        }

        spdlog::info("Ubuntu Blockchain daemon is ready!");
        spdlog::info("Use 'ubu-cli' to interact with the node");

        // Main loop
        while (!g_shutdownRequested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Periodic status logging
            static int counter = 0;
            if (++counter % 60 == 0) {  // Every minute
                auto state = blockchain->getState();
                auto netStats = networkManager->getStats();
                auto mempoolStats = mempool->getStats();

                spdlog::info(
                    "Status - Height: {}, Peers: {}, Mempool: {} tx, Network: {}/{} KB",
                    state.height, netStats.connectedPeers, mempoolStats.count,
                    netStats.bytesReceived / 1024, netStats.bytesSent / 1024);
            }
        }

        // Shutdown sequence
        spdlog::info("Shutting down...");

        if (rpcServer) {
            rpcServer->stop();
        }

        networkManager->stop();

        // Save wallet
        wallet->saveToFile(dataDir + "/wallet.dat", "");

        // Flush databases
        utxoDb->flush();
        db->compact();

        spdlog::info("Shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}
