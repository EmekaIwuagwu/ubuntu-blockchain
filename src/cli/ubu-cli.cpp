/**
 * Ubuntu Blockchain CLI Tool
 *
 * Command-line interface for interacting with ubud daemon via RPC.
 */

#include "ubuntu/rpc/rpc_server.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ubuntu::rpc;

/**
 * @brief Send RPC command to daemon
 *
 * @param host RPC server host
 * @param port RPC server port
 * @param method RPC method name
 * @param params Method parameters
 * @return Response JSON string
 */
std::string sendRpcCommand(const std::string& host, uint16_t port, const std::string& method,
                            const std::vector<std::string>& params) {
    // Build JSON-RPC request
    JsonValue request = JsonValue::makeObject();
    request.set("jsonrpc", JsonValue("2.0"));
    request.set("id", JsonValue(static_cast<int64_t>(1)));
    request.set("method", JsonValue(method));

    JsonValue paramsArray = JsonValue::makeArray();
    for (const auto& param : params) {
        // Try to parse as number
        try {
            if (param.find('.') != std::string::npos) {
                paramsArray.pushBack(JsonValue(std::stod(param)));
            } else {
                paramsArray.pushBack(JsonValue(std::stoll(param)));
            }
        } catch (...) {
            // Not a number, treat as string
            paramsArray.pushBack(JsonValue(param));
        }
    }
    request.set("params", paramsArray);

    std::string requestStr = request.toJsonString();

    // In production, send HTTP request to daemon
    // For now, simulate response
    std::cout << "Sending RPC request to " << host << ":" << port << std::endl;
    std::cout << "Request: " << requestStr << std::endl;

    // Placeholder response
    JsonValue response = JsonValue::makeObject();
    response.set("jsonrpc", JsonValue("2.0"));
    response.set("id", JsonValue(static_cast<int64_t>(1)));
    response.set("result", JsonValue("RPC call successful (simulated)"));

    return response.toJsonString();
}

void printUsage(const char* programName) {
    std::cout << "Ubuntu Blockchain CLI\n";
    std::cout << "Usage: " << programName << " [options] <command> [params]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -rpchost=<host>      RPC server host (default: 127.0.0.1)\n";
    std::cout << "  -rpcport=<port>      RPC server port (default: 8332)\n";
    std::cout << "  -rpcuser=<username>  RPC username\n";
    std::cout << "  -rpcpassword=<pass>  RPC password\n";
    std::cout << "  -h, --help           Show this help message\n\n";
    std::cout << "Common commands:\n";
    std::cout << "  getblockchaininfo    Get blockchain status\n";
    std::cout << "  getblockcount        Get current block height\n";
    std::cout << "  getblock <hash>      Get block by hash\n";
    std::cout << "  getnewaddress        Generate new address\n";
    std::cout << "  getbalance           Get wallet balance\n";
    std::cout << "  sendtoaddress        Send UBU to address\n";
    std::cout << "  listtransactions     List wallet transactions\n";
    std::cout << "  getpeerinfo          Get connected peers\n";
    std::cout << "  stop                 Stop the daemon\n\n";
    std::cout << "For more commands, see: https://docs.ubuntublockchain.xyz/rpc\n";
}

int main(int argc, char* argv[]) {
    // Default RPC connection settings
    std::string rpcHost = "127.0.0.1";
    uint16_t rpcPort = 8332;
    std::string rpcUser = "";
    std::string rpcPassword = "";

    // Parse command line
    std::vector<std::string> args;
    std::string method;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg.find("-rpchost=") == 0) {
            rpcHost = arg.substr(9);
        } else if (arg.find("-rpcport=") == 0) {
            rpcPort = static_cast<uint16_t>(std::stoi(arg.substr(9)));
        } else if (arg.find("-rpcuser=") == 0) {
            rpcUser = arg.substr(9);
        } else if (arg.find("-rpcpassword=") == 0) {
            rpcPassword = arg.substr(13);
        } else {
            if (method.empty()) {
                method = arg;
            } else {
                args.push_back(arg);
            }
        }
    }

    if (method.empty()) {
        std::cerr << "Error: No command specified\n\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        // Send RPC command
        std::string response = sendRpcCommand(rpcHost, rpcPort, method, args);

        // Parse and display response
        JsonValue responseJson = JsonValue::fromJsonString(response);

        if (responseJson.has("error") && !responseJson["error"].isNull()) {
            std::cerr << "RPC Error: " << responseJson["error"]["message"].getString()
                      << std::endl;
            return 1;
        }

        if (responseJson.has("result")) {
            std::cout << "Result:\n" << responseJson["result"].toJsonString() << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
