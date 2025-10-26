#pragma once

#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/storage/block_index.h"
#include "ubuntu/storage/utxo_db.h"
#include "ubuntu/mempool/mempool.h"

#include <memory>

namespace ubuntu {
namespace rpc {

/**
 * @brief Blockchain Explorer RPC Methods
 *
 * Provides comprehensive API for blockchain explorers to query:
 * - Address balances and transaction history
 * - Block details with full transaction data
 * - Network statistics and metrics
 * - Rich list and distribution
 * - Search functionality
 */
class ExplorerRpc {
public:
    ExplorerRpc(std::shared_ptr<core::Blockchain> blockchain,
                std::shared_ptr<storage::BlockIndex> blockIndex,
                std::shared_ptr<storage::UTXODatabase> utxoDb,
                std::shared_ptr<mempool::Mempool> mempool);

    ~ExplorerRpc();

    /**
     * @brief Register all explorer RPC methods
     */
    void registerMethods(RpcServer& server);

private:
    // Address queries
    JsonValue getAddressBalance(const JsonValue& params);
    JsonValue getAddressTransactions(const JsonValue& params);
    JsonValue getAddressUTXOs(const JsonValue& params);

    // Block queries
    JsonValue getBlockByHeight(const JsonValue& params);
    JsonValue getBlocksByRange(const JsonValue& params);
    JsonValue getLatestBlocks(const JsonValue& params);
    JsonValue getBlockReward(const JsonValue& params);

    // Transaction queries
    JsonValue getTransactionDetails(const JsonValue& params);
    JsonValue searchTransactions(const JsonValue& params);
    JsonValue getPendingTransactions(const JsonValue& params);

    // Network statistics
    JsonValue getNetworkStats(const JsonValue& params);
    JsonValue getSupplyInfo(const JsonValue& params);
    JsonValue getRichList(const JsonValue& params);
    JsonValue getDistributionStats(const JsonValue& params);

    // Search
    JsonValue search(const JsonValue& params);

    // Helper methods
    JsonValue blockToJson(const core::Block& block, bool includeTransactions);
    JsonValue transactionToJson(const core::Transaction& tx, const crypto::Hash256& blockHash,
                               uint32_t blockHeight, uint32_t confirmations);

    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<storage::BlockIndex> blockIndex_;
    std::shared_ptr<storage::UTXODatabase> utxoDb_;
    std::shared_ptr<mempool::Mempool> mempool_;
};

}  // namespace rpc
}  // namespace ubuntu
