#include "ubuntu/rpc/wallet_rpc.h"

#include <spdlog/spdlog.h>

namespace ubuntu {
namespace rpc {

WalletRpc::WalletRpc(std::shared_ptr<wallet::Wallet> wallet,
                     std::shared_ptr<mempool::FeeEstimator> feeEstimator,
                     std::shared_ptr<network::NetworkManager> networkManager)
    : wallet_(wallet), feeEstimator_(feeEstimator), networkManager_(networkManager) {}

WalletRpc::~WalletRpc() = default;

void WalletRpc::registerMethods(RpcServer& server) {
    // Wallet info
    server.registerMethod("getwalletinfo",
                         [this](const JsonValue& p) { return getWalletInfo(p); });

    // Address methods
    server.registerMethod("getnewaddress",
                         [this](const JsonValue& p) { return getNewAddress(p); });
    server.registerMethod("getaddressesbylabel",
                         [this](const JsonValue& p) { return getAddressesByLabel(p); });
    server.registerMethod("listaddresses",
                         [this](const JsonValue& p) { return listAddresses(p); });

    // Balance methods
    server.registerMethod("getbalance", [this](const JsonValue& p) { return getBalance(p); });
    server.registerMethod("getunconfirmedbalance",
                         [this](const JsonValue& p) { return getUnconfirmedBalance(p); });

    // Transaction methods
    server.registerMethod("sendtoaddress",
                         [this](const JsonValue& p) { return sendToAddress(p); });
    server.registerMethod("sendmany", [this](const JsonValue& p) { return sendMany(p); });
    server.registerMethod("listtransactions",
                         [this](const JsonValue& p) { return listTransactions(p); });
    server.registerMethod("gettransaction",
                         [this](const JsonValue& p) { return getTransaction(p); });

    // Key management
    server.registerMethod("dumpprivkey", [this](const JsonValue& p) { return dumpPrivKey(p); });
    server.registerMethod("importprivkey",
                         [this](const JsonValue& p) { return importPrivKey(p); });

    // Backup
    server.registerMethod("backupwallet",
                         [this](const JsonValue& p) { return backupWallet(p); });

    spdlog::info("Registered {} wallet RPC methods", 13);
}

JsonValue WalletRpc::getWalletInfo(const JsonValue& params) {
    JsonValue result = JsonValue::makeObject();

    result.set("walletname", JsonValue("wallet.dat"));
    result.set("walletversion", JsonValue(static_cast<int64_t>(1)));

    uint64_t balance = wallet_->getBalance();
    result.set("balance", amountToJson(balance));

    uint64_t unconfirmed = wallet_->getUnconfirmedBalance();
    result.set("unconfirmed_balance", amountToJson(unconfirmed));

    auto addresses = wallet_->getAddresses(false);
    result.set("keypoolsize", JsonValue(static_cast<int64_t>(addresses.size())));

    return result;
}

JsonValue WalletRpc::getNewAddress(const JsonValue& params) {
    std::string label = "";
    if (params.isArray() && params.size() > 0) {
        label = params[0].getString();
    }

    wallet::AddressType type = wallet::AddressType::P2PKH;
    if (params.isArray() && params.size() > 1) {
        std::string typeStr = params[1].getString();
        if (typeStr == "bech32") {
            type = wallet::AddressType::BECH32;
        }
    }

    std::string address = wallet_->getNewAddress(label, type);
    return JsonValue(address);
}

JsonValue WalletRpc::getAddressesByLabel(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected label");
    }

    std::string label = params[0].getString();

    JsonValue result = JsonValue::makeObject();

    auto addresses = wallet_->getAddresses(false);
    for (const auto& addr : addresses) {
        if (addr.label == label) {
            JsonValue addrInfo = JsonValue::makeObject();
            addrInfo.set("purpose", JsonValue("receive"));
            result.set(addr.address, addrInfo);
        }
    }

    return result;
}

JsonValue WalletRpc::listAddresses(const JsonValue& params) {
    JsonValue result = JsonValue::makeArray();

    auto addresses = wallet_->getAddresses(false);
    for (const auto& addr : addresses) {
        JsonValue addrInfo = JsonValue::makeObject();
        addrInfo.set("address", JsonValue(addr.address));
        addrInfo.set("label", JsonValue(addr.label));

        std::string typeStr;
        switch (addr.type) {
            case wallet::AddressType::P2PKH:
                typeStr = "p2pkh";
                break;
            case wallet::AddressType::BECH32:
                typeStr = "bech32";
                break;
            case wallet::AddressType::P2SH:
                typeStr = "p2sh";
                break;
        }
        addrInfo.set("type", JsonValue(typeStr));

        result.pushBack(addrInfo);
    }

    return result;
}

JsonValue WalletRpc::getBalance(const JsonValue& params) {
    uint32_t minConf = 1;
    if (params.isArray() && params.size() > 0) {
        minConf = static_cast<uint32_t>(params[0].getInt());
    }

    uint64_t balance = wallet_->getBalance(minConf);
    return amountToJson(balance);
}

JsonValue WalletRpc::getUnconfirmedBalance(const JsonValue& params) {
    uint64_t balance = wallet_->getUnconfirmedBalance();
    return amountToJson(balance);
}

JsonValue WalletRpc::sendToAddress(const JsonValue& params) {
    if (!params.isArray() || params.size() < 2) {
        throw std::runtime_error("Invalid parameters: expected address and amount");
    }

    std::string toAddress = params[0].getString();
    uint64_t amount = parseAmount(params[1]);

    std::string comment = "";
    if (params.size() > 2) {
        comment = params[2].getString();
    }

    // Get fee rate
    uint64_t feeRate = 1000;  // Default 1000 sat/kB
    if (feeEstimator_) {
        auto estimatedRate = feeEstimator_->estimateFeeRate(6);
        if (estimatedRate) {
            feeRate = *estimatedRate;
        }
    }

    // Create transaction
    std::map<std::string, uint64_t> recipients = {{toAddress, amount}};
    auto tx = wallet_->createTransaction(recipients, feeRate, false);

    // Calculate actual fee
    uint64_t totalInput = 0;
    uint64_t totalOutput = 0;

    for (const auto& input : tx.inputs) {
        // Get input amount from wallet's UTXO
        auto utxo = wallet_->getUTXO(input.previousOutput);
        if (utxo) {
            totalInput += utxo->output.value;
        }
    }

    for (const auto& output : tx.outputs) {
        totalOutput += output.value;
    }

    uint64_t actualFee = totalInput - totalOutput;

    // Get from addresses (inputs)
    JsonValue fromAddresses = JsonValue::makeArray();
    for (const auto& input : tx.inputs) {
        auto utxo = wallet_->getUTXO(input.previousOutput);
        if (utxo) {
            // Extract address from scriptPubKey
            std::string fromAddr = wallet_->extractAddressFromScript(utxo->output.scriptPubKey);
            if (!fromAddr.empty()) {
                fromAddresses.pushBack(JsonValue(fromAddr));
            }
        }
    }

    // Broadcast transaction
    if (networkManager_) {
        networkManager_->broadcastTransaction(tx);
    }

    // Add to wallet
    wallet_->addTransaction(tx, 0);

    spdlog::info("Sent {} UBU to {} (fee: {} UBU)",
                 amount / 100000000.0, toAddress, actualFee / 100000000.0);

    // Create detailed receipt
    JsonValue result = JsonValue::makeObject();
    result.set("txid", JsonValue(tx.getHash().toHex()));
    result.set("from", fromAddresses);
    result.set("to", JsonValue(toAddress));
    result.set("amount", amountToJson(amount));
    result.set("fee", amountToJson(actualFee));
    result.set("total_deducted", amountToJson(amount + actualFee));
    result.set("timestamp", JsonValue(static_cast<int64_t>(std::time(nullptr))));
    result.set("size", JsonValue(static_cast<int64_t>(tx.getSize())));
    result.set("confirmations", JsonValue(static_cast<int64_t>(0)));
    result.set("status", JsonValue("pending"));

    if (!comment.empty()) {
        result.set("comment", JsonValue(comment));
    }

    return result;
}

JsonValue WalletRpc::sendMany(const JsonValue& params) {
    if (!params.isArray() || params.size() < 2) {
        throw std::runtime_error("Invalid parameters: expected recipients");
    }

    // Skip account (deprecated parameter)
    const JsonValue& recipientsJson = params[1];

    if (!recipientsJson.isObject()) {
        throw std::runtime_error("Recipients must be an object");
    }

    std::map<std::string, uint64_t> recipients;
    for (const auto& key : recipientsJson.keys()) {
        recipients[key] = parseAmount(recipientsJson[key]);
    }

    // Get fee rate
    uint64_t feeRate = 1000;
    if (feeEstimator_) {
        auto estimatedRate = feeEstimator_->estimateFeeRate(6);
        if (estimatedRate) {
            feeRate = *estimatedRate;
        }
    }

    // Create transaction
    auto tx = wallet_->createTransaction(recipients, feeRate, false);

    // Broadcast transaction
    if (networkManager_) {
        networkManager_->broadcastTransaction(tx);
    }

    // Add to wallet
    wallet_->addTransaction(tx, 0);

    spdlog::info("Sent to {} recipients", recipients.size());

    return JsonValue(tx.getHash().toHex());
}

JsonValue WalletRpc::listTransactions(const JsonValue& params) {
    size_t count = 10;
    size_t skip = 0;

    if (params.isArray() && params.size() > 0) {
        // Skip account parameter
        if (params.size() > 1) {
            count = static_cast<size_t>(params[1].getInt());
        }
        if (params.size() > 2) {
            skip = static_cast<size_t>(params[2].getInt());
        }
    }

    auto transactions = wallet_->listTransactions(count, skip);

    JsonValue result = JsonValue::makeArray();
    for (const auto& wtx : transactions) {
        JsonValue txInfo = JsonValue::makeObject();

        txInfo.set("txid", JsonValue(wtx.txHash.toHex()));
        txInfo.set("amount", amountToJson(std::abs(wtx.amount)));
        txInfo.set("confirmations",
                   JsonValue(static_cast<int64_t>(wtx.blockHeight > 0 ? 1 : 0)));
        txInfo.set("time", JsonValue(static_cast<int64_t>(wtx.timestamp)));

        if (wtx.amount > 0) {
            txInfo.set("category", JsonValue("receive"));
        } else {
            txInfo.set("category", JsonValue("send"));
        }

        if (!wtx.comment.empty()) {
            txInfo.set("comment", JsonValue(wtx.comment));
        }

        result.pushBack(txInfo);
    }

    return result;
}

JsonValue WalletRpc::getTransaction(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected transaction hash");
    }

    std::string hashStr = params[0].getString();
    crypto::Hash256 hash = crypto::Hash256::fromHex(hashStr);

    auto wtx = wallet_->getTransaction(hash);
    if (!wtx) {
        throw std::runtime_error("Transaction not found in wallet");
    }

    JsonValue result = JsonValue::makeObject();
    result.set("txid", JsonValue(wtx->txHash.toHex()));
    result.set("amount", amountToJson(std::abs(wtx->amount)));
    result.set("confirmations",
               JsonValue(static_cast<int64_t>(wtx->blockHeight > 0 ? 1 : 0)));
    result.set("time", JsonValue(static_cast<int64_t>(wtx->timestamp)));

    if (wtx->amount > 0) {
        result.set("category", JsonValue("receive"));
    } else {
        result.set("category", JsonValue("send"));
    }

    return result;
}

JsonValue WalletRpc::dumpPrivKey(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected address");
    }

    std::string address = params[0].getString();

    auto privKey = wallet_->getPrivateKey(address);
    if (!privKey) {
        throw std::runtime_error("Address not found in wallet");
    }

    return JsonValue(privKey->toWIF());
}

JsonValue WalletRpc::importPrivKey(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected private key");
    }

    std::string wif = params[0].getString();

    // In production, import the private key
    // For now, placeholder
    spdlog::warn("importprivkey not fully implemented");

    return JsonValue();
}

JsonValue WalletRpc::backupWallet(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected destination");
    }

    std::string destination = params[0].getString();

    bool success = wallet_->saveToFile(destination, "");

    if (!success) {
        throw std::runtime_error("Failed to backup wallet");
    }

    return JsonValue();
}

uint64_t WalletRpc::parseAmount(const JsonValue& value) const {
    double amount = value.getDouble();
    return static_cast<uint64_t>(amount * 100000000.0);  // Convert UBU to satoshis
}

JsonValue WalletRpc::amountToJson(uint64_t amount) const {
    double ubu = static_cast<double>(amount) / 100000000.0;
    return JsonValue(ubu);
}

}  // namespace rpc
}  // namespace ubuntu
