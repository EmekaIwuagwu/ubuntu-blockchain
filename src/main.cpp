/**
 * @file main.cpp
 * @brief Ubuntu Blockchain Node Daemon (ubud)
 *
 * Main entry point for the Ubuntu Blockchain full node.
 */

#include "ubuntu/core/block.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"

#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    try {
        spdlog::info("Ubuntu Blockchain Node (ubud) v1.0.0");
        spdlog::info("====================================");

        // Initialize logging
        spdlog::set_level(spdlog::level::info);

        // Parse command line arguments
        // TODO: Implement full argument parsing

        // Display startup information
        spdlog::info("Initializing Ubuntu Blockchain...");

        // Create genesis block
        auto genesis = ubuntu::core::createGenesisBlock();
        spdlog::info("Genesis block hash: {}", genesis.calculateHash().toHex());

        // Test cryptography
        auto keyPair = ubuntu::crypto::KeyPair::generate();
        spdlog::info("Generated key pair successfully");
        spdlog::info("Public key: {}", keyPair.publicKey.toHex());

        // TODO: Initialize components
        // - Database
        // - P2P network
        // - Mempool
        // - Miner (if enabled)
        // - RPC server

        spdlog::info("Node initialization complete");
        spdlog::info("Node is ready (placeholder - full implementation pending)");

        // Main event loop would go here
        // For now, just exit
        spdlog::info("Press Ctrl+C to stop the node");

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}
