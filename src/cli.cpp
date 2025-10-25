/**
 * @file cli.cpp
 * @brief Ubuntu Blockchain CLI Wallet (ubu-cli)
 */

#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    try {
        spdlog::info("Ubuntu Blockchain CLI v1.0.0");
        
        if (argc < 2) {
            std::cout << "Usage: ubu-cli <command> [options]" << std::endl;
            std::cout << "Commands:" << std::endl;
            std::cout << "  getnewaddress  - Generate a new address" << std::endl;
            std::cout << "  getbalance     - Get wallet balance" << std::endl;
            std::cout << "  help           - Show this help message" << std::endl;
            return 0;
        }
        
        std::string command = argv[1];
        
        if (command == "help") {
            std::cout << "Ubuntu Blockchain CLI - Wallet interface" << std::endl;
        } else if (command == "getnewaddress") {
            auto keyPair = ubuntu::crypto::KeyPair::generate();
            std::cout << "Public key: " << keyPair.publicKey.toHex() << std::endl;
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            return 1;
        }
        
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
