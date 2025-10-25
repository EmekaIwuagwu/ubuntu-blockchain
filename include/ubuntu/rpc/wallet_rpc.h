#pragma once

#include "ubuntu/mempool/fee_estimator.h"
#include "ubuntu/network/network_manager.h"
#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/wallet/wallet.h"

#include <memory>

namespace ubuntu {
namespace rpc {

/**
 * @brief Wallet RPC methods
 *
 * Provides RPC methods for wallet management and transactions.
 */
class WalletRpc {
public:
    /**
     * @brief Construct wallet RPC handler
     *
     * @param wallet Wallet instance
     * @param feeEstimator Fee estimator
     * @param networkManager Network manager for broadcasting
     */
    WalletRpc(std::shared_ptr<wallet::Wallet> wallet,
              std::shared_ptr<mempool::FeeEstimator> feeEstimator,
              std::shared_ptr<network::NetworkManager> networkManager);

    ~WalletRpc();

    /**
     * @brief Register all wallet RPC methods
     *
     * @param server RPC server to register methods with
     */
    void registerMethods(RpcServer& server);

private:
    std::shared_ptr<wallet::Wallet> wallet_;
    std::shared_ptr<mempool::FeeEstimator> feeEstimator_;
    std::shared_ptr<network::NetworkManager> networkManager_;

    // Wallet management methods
    JsonValue getWalletInfo(const JsonValue& params);
    JsonValue getNewAddress(const JsonValue& params);
    JsonValue getAddressesByLabel(const JsonValue& params);
    JsonValue listAddresses(const JsonValue& params);

    // Balance methods
    JsonValue getBalance(const JsonValue& params);
    JsonValue getUnconfirmedBalance(const JsonValue& params);

    // Transaction methods
    JsonValue sendToAddress(const JsonValue& params);
    JsonValue sendMany(const JsonValue& params);
    JsonValue listTransactions(const JsonValue& params);
    JsonValue getTransaction(const JsonValue& params);

    // Key management methods
    JsonValue dumpPrivKey(const JsonValue& params);
    JsonValue importPrivKey(const JsonValue& params);

    // Wallet backup methods
    JsonValue backupWallet(const JsonValue& params);

    // Helper methods
    uint64_t parseAmount(const JsonValue& value) const;
    JsonValue amountToJson(uint64_t amount) const;
};

}  // namespace rpc
}  // namespace ubuntu
