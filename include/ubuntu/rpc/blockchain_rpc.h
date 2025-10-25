#pragma once

#include "ubuntu/core/chain.h"
#include "ubuntu/mempool/mempool.h"
#include "ubuntu/network/network_manager.h"
#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/storage/block_index.h"

#include <memory>

namespace ubuntu {
namespace rpc {

/**
 * @brief Blockchain RPC methods
 *
 * Provides RPC methods for blockchain information and control.
 */
class BlockchainRpc {
public:
    /**
     * @brief Construct blockchain RPC handler
     *
     * @param blockchain Blockchain instance
     * @param blockStorage Block storage
     * @param mempool Mempool
     * @param networkManager Network manager
     */
    BlockchainRpc(std::shared_ptr<core::Blockchain> blockchain,
                  std::shared_ptr<storage::BlockStorage> blockStorage,
                  std::shared_ptr<mempool::Mempool> mempool,
                  std::shared_ptr<network::NetworkManager> networkManager);

    ~BlockchainRpc();

    /**
     * @brief Register all blockchain RPC methods
     *
     * @param server RPC server to register methods with
     */
    void registerMethods(RpcServer& server);

private:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<storage::BlockStorage> blockStorage_;
    std::shared_ptr<mempool::Mempool> mempool_;
    std::shared_ptr<network::NetworkManager> networkManager_;

    // Blockchain information methods
    JsonValue getBlockchainInfo(const JsonValue& params);
    JsonValue getBlockCount(const JsonValue& params);
    JsonValue getBestBlockHash(const JsonValue& params);
    JsonValue getDifficulty(const JsonValue& params);
    JsonValue getChainTips(const JsonValue& params);

    // Block methods
    JsonValue getBlock(const JsonValue& params);
    JsonValue getBlockHash(const JsonValue& params);
    JsonValue getBlockHeader(const JsonValue& params);

    // Transaction methods
    JsonValue getRawTransaction(const JsonValue& params);
    JsonValue decodeRawTransaction(const JsonValue& params);
    JsonValue sendRawTransaction(const JsonValue& params);

    // Mempool methods
    JsonValue getMempoolInfo(const JsonValue& params);
    JsonValue getRawMempool(const JsonValue& params);
    JsonValue getMempoolEntry(const JsonValue& params);

    // Network methods
    JsonValue getConnectionCount(const JsonValue& params);
    JsonValue getPeerInfo(const JsonValue& params);
    JsonValue getNetworkInfo(const JsonValue& params);

    // Utility methods
    JsonValue validateAddress(const JsonValue& params);
    JsonValue estimateFee(const JsonValue& params);

    // Helper methods
    JsonValue blockToJson(const core::Block& block, bool verbose) const;
    JsonValue blockHeaderToJson(const core::BlockHeader& header) const;
    JsonValue transactionToJson(const core::Transaction& tx, bool verbose) const;
};

}  // namespace rpc
}  // namespace ubuntu
