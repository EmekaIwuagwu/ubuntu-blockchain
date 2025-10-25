#include "ubuntu/rpc/blockchain_rpc.h"

#include <spdlog/spdlog.h>

namespace ubuntu {
namespace rpc {

BlockchainRpc::BlockchainRpc(std::shared_ptr<core::Blockchain> blockchain,
                             std::shared_ptr<storage::BlockStorage> blockStorage,
                             std::shared_ptr<mempool::Mempool> mempool,
                             std::shared_ptr<network::NetworkManager> networkManager)
    : blockchain_(blockchain),
      blockStorage_(blockStorage),
      mempool_(mempool),
      networkManager_(networkManager) {}

BlockchainRpc::~BlockchainRpc() = default;

void BlockchainRpc::registerMethods(RpcServer& server) {
    // Blockchain info
    server.registerMethod("getblockchaininfo",
                         [this](const JsonValue& p) { return getBlockchainInfo(p); });
    server.registerMethod("getblockcount", [this](const JsonValue& p) { return getBlockCount(p); });
    server.registerMethod("getbestblockhash",
                         [this](const JsonValue& p) { return getBestBlockHash(p); });
    server.registerMethod("getdifficulty", [this](const JsonValue& p) { return getDifficulty(p); });
    server.registerMethod("getchaintips", [this](const JsonValue& p) { return getChainTips(p); });

    // Blocks
    server.registerMethod("getblock", [this](const JsonValue& p) { return getBlock(p); });
    server.registerMethod("getblockhash", [this](const JsonValue& p) { return getBlockHash(p); });
    server.registerMethod("getblockheader",
                         [this](const JsonValue& p) { return getBlockHeader(p); });

    // Transactions
    server.registerMethod("getrawtransaction",
                         [this](const JsonValue& p) { return getRawTransaction(p); });
    server.registerMethod("decoderawtransaction",
                         [this](const JsonValue& p) { return decodeRawTransaction(p); });
    server.registerMethod("sendrawtransaction",
                         [this](const JsonValue& p) { return sendRawTransaction(p); });

    // Mempool
    server.registerMethod("getmempoolinfo",
                         [this](const JsonValue& p) { return getMempoolInfo(p); });
    server.registerMethod("getrawmempool",
                         [this](const JsonValue& p) { return getRawMempool(p); });
    server.registerMethod("getmempoolentry",
                         [this](const JsonValue& p) { return getMempoolEntry(p); });

    // Network
    server.registerMethod("getconnectioncount",
                         [this](const JsonValue& p) { return getConnectionCount(p); });
    server.registerMethod("getpeerinfo", [this](const JsonValue& p) { return getPeerInfo(p); });
    server.registerMethod("getnetworkinfo",
                         [this](const JsonValue& p) { return getNetworkInfo(p); });

    // Utility
    server.registerMethod("validateaddress",
                         [this](const JsonValue& p) { return validateAddress(p); });
    server.registerMethod("estimatefee", [this](const JsonValue& p) { return estimateFee(p); });

    spdlog::info("Registered {} blockchain RPC methods", 18);
}

JsonValue BlockchainRpc::getBlockchainInfo(const JsonValue& params) {
    auto state = blockchain_->getState();
    auto tip = blockchain_->getTip();

    JsonValue result = JsonValue::makeObject();
    result.set("chain", JsonValue("main"));
    result.set("blocks", JsonValue(static_cast<int64_t>(state.height)));
    result.set("headers", JsonValue(static_cast<int64_t>(state.height)));

    if (tip) {
        result.set("bestblockhash", JsonValue(tip->getHash().toHex()));
    }

    result.set("difficulty", JsonValue(static_cast<double>(state.target)));
    result.set("chainwork", JsonValue(static_cast<int64_t>(state.chainWork)));

    return result;
}

JsonValue BlockchainRpc::getBlockCount(const JsonValue& params) {
    auto state = blockchain_->getState();
    return JsonValue(static_cast<int64_t>(state.height));
}

JsonValue BlockchainRpc::getBestBlockHash(const JsonValue& params) {
    auto tip = blockchain_->getTip();
    if (!tip) {
        throw std::runtime_error("No blockchain tip");
    }
    return JsonValue(tip->getHash().toHex());
}

JsonValue BlockchainRpc::getDifficulty(const JsonValue& params) {
    auto state = blockchain_->getState();
    return JsonValue(static_cast<double>(state.target));
}

JsonValue BlockchainRpc::getChainTips(const JsonValue& params) {
    JsonValue tips = JsonValue::makeArray();

    auto tip = blockchain_->getTip();
    if (tip) {
        JsonValue tipInfo = JsonValue::makeObject();
        tipInfo.set("height", JsonValue(static_cast<int64_t>(blockchain_->getState().height)));
        tipInfo.set("hash", JsonValue(tip->getHash().toHex()));
        tipInfo.set("status", JsonValue("active"));
        tips.pushBack(tipInfo);
    }

    return tips;
}

JsonValue BlockchainRpc::getBlock(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected block hash");
    }

    std::string hashStr = params[0].getString();
    crypto::Hash256 hash = crypto::Hash256::fromHex(hashStr);

    auto block = blockStorage_->getBlock(hash);
    if (!block) {
        throw std::runtime_error("Block not found");
    }

    bool verbose = true;
    if (params.size() > 1 && params[1].isBool()) {
        verbose = params[1].getBool();
    }

    return blockToJson(*block, verbose);
}

JsonValue BlockchainRpc::getBlockHash(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected block height");
    }

    uint32_t height = static_cast<uint32_t>(params[0].getInt());

    auto block = blockStorage_->getBlockByHeight(height);
    if (!block) {
        throw std::runtime_error("Block not found");
    }

    return JsonValue(block->getHash().toHex());
}

JsonValue BlockchainRpc::getBlockHeader(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected block hash");
    }

    std::string hashStr = params[0].getString();
    crypto::Hash256 hash = crypto::Hash256::fromHex(hashStr);

    auto block = blockStorage_->getBlock(hash);
    if (!block) {
        throw std::runtime_error("Block not found");
    }

    return blockHeaderToJson(block->header);
}

JsonValue BlockchainRpc::getRawTransaction(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected transaction hash");
    }

    std::string hashStr = params[0].getString();
    crypto::Hash256 hash = crypto::Hash256::fromHex(hashStr);

    // Check mempool first
    auto tx = mempool_->getTransaction(hash);
    if (tx) {
        bool verbose = false;
        if (params.size() > 1 && params[1].isBool()) {
            verbose = params[1].getBool();
        }
        return transactionToJson(*tx, verbose);
    }

    throw std::runtime_error("Transaction not found");
}

JsonValue BlockchainRpc::decodeRawTransaction(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected raw transaction hex");
    }

    std::string hexStr = params[0].getString();
    auto txData = crypto::Hash256::fromHex(hexStr);

    // In production, deserialize transaction from hex
    // For now, return placeholder
    JsonValue result = JsonValue::makeObject();
    result.set("txid", JsonValue(hexStr));

    return result;
}

JsonValue BlockchainRpc::sendRawTransaction(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected raw transaction hex");
    }

    std::string hexStr = params[0].getString();

    // In production, deserialize and broadcast transaction
    // For now, return placeholder transaction hash
    return JsonValue(hexStr);
}

JsonValue BlockchainRpc::getMempoolInfo(const JsonValue& params) {
    auto stats = mempool_->getStats();

    JsonValue result = JsonValue::makeObject();
    result.set("size", JsonValue(static_cast<int64_t>(stats.count)));
    result.set("bytes", JsonValue(static_cast<int64_t>(stats.totalSize)));
    result.set("usage", JsonValue(static_cast<int64_t>(stats.totalSize)));
    result.set("maxmempool", JsonValue(static_cast<int64_t>(300 * 1024 * 1024)));
    result.set("mempoolminfee", JsonValue(static_cast<double>(stats.minFeeRate)));

    return result;
}

JsonValue BlockchainRpc::getRawMempool(const JsonValue& params) {
    bool verbose = false;
    if (params.isArray() && params.size() > 0 && params[0].isBool()) {
        verbose = params[0].getBool();
    }

    auto transactions = mempool_->getAllTransactions();

    if (verbose) {
        JsonValue result = JsonValue::makeObject();
        for (const auto& tx : transactions) {
            JsonValue txInfo = JsonValue::makeObject();
            txInfo.set("size", JsonValue(static_cast<int64_t>(tx.getSize())));
            txInfo.set("fee", JsonValue(0.0));  // Placeholder
            result.set(tx.getHash().toHex(), txInfo);
        }
        return result;
    } else {
        JsonValue result = JsonValue::makeArray();
        for (const auto& tx : transactions) {
            result.pushBack(JsonValue(tx.getHash().toHex()));
        }
        return result;
    }
}

JsonValue BlockchainRpc::getMempoolEntry(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected transaction hash");
    }

    std::string hashStr = params[0].getString();
    crypto::Hash256 hash = crypto::Hash256::fromHex(hashStr);

    auto tx = mempool_->getTransaction(hash);
    if (!tx) {
        throw std::runtime_error("Transaction not in mempool");
    }

    JsonValue result = JsonValue::makeObject();
    result.set("size", JsonValue(static_cast<int64_t>(tx->getSize())));
    result.set("fee", JsonValue(0.0));  // Placeholder

    return result;
}

JsonValue BlockchainRpc::getConnectionCount(const JsonValue& params) {
    size_t count = networkManager_->getPeerCount();
    return JsonValue(static_cast<int64_t>(count));
}

JsonValue BlockchainRpc::getPeerInfo(const JsonValue& params) {
    auto peers = networkManager_->getPeers();

    JsonValue result = JsonValue::makeArray();
    for (const auto& peer : peers) {
        JsonValue peerInfo = JsonValue::makeObject();
        peerInfo.set("addr", JsonValue(peer.toString()));
        peerInfo.set("services", JsonValue(static_cast<int64_t>(peer.services)));
        result.pushBack(peerInfo);
    }

    return result;
}

JsonValue BlockchainRpc::getNetworkInfo(const JsonValue& params) {
    auto stats = networkManager_->getStats();

    JsonValue result = JsonValue::makeObject();
    result.set("version", JsonValue(static_cast<int64_t>(70015)));
    result.set("subversion", JsonValue("/UbuntuBlockchain:1.0.0/"));
    result.set("connections", JsonValue(static_cast<int64_t>(stats.connectedPeers)));

    return result;
}

JsonValue BlockchainRpc::validateAddress(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected address");
    }

    std::string address = params[0].getString();

    JsonValue result = JsonValue::makeObject();
    result.set("isvalid", JsonValue(false));  // Simplified validation
    result.set("address", JsonValue(address));

    return result;
}

JsonValue BlockchainRpc::estimateFee(const JsonValue& params) {
    if (!params.isArray() || params.size() < 1) {
        throw std::runtime_error("Invalid parameters: expected number of blocks");
    }

    int64_t nblocks = params[0].getInt();

    // Return estimated fee rate
    JsonValue result = JsonValue::makeObject();
    result.set("feerate", JsonValue(0.00001));  // Placeholder: 1000 sat/kB

    return result;
}

JsonValue BlockchainRpc::blockToJson(const core::Block& block, bool verbose) const {
    if (!verbose) {
        auto serialized = block.serialize();
        // Convert to hex string
        std::string hex;
        for (uint8_t byte : serialized) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            hex += buf;
        }
        return JsonValue(hex);
    }

    JsonValue result = JsonValue::makeObject();
    result.set("hash", JsonValue(block.getHash().toHex()));
    result.set("confirmations", JsonValue(1));  // Simplified
    result.set("size", JsonValue(static_cast<int64_t>(block.getSize())));
    result.set("height", JsonValue(static_cast<int64_t>(0)));  // Would need block index
    result.set("version", JsonValue(static_cast<int64_t>(block.header.version)));
    result.set("merkleroot", JsonValue(block.header.merkleRoot.toHex()));
    result.set("time", JsonValue(static_cast<int64_t>(block.header.timestamp)));
    result.set("nonce", JsonValue(static_cast<int64_t>(block.header.nonce)));
    result.set("bits", JsonValue(block.header.bits));
    result.set("difficulty", JsonValue(static_cast<double>(block.header.bits)));
    result.set("previousblockhash", JsonValue(block.header.previousBlockHash.toHex()));

    JsonValue txArray = JsonValue::makeArray();
    for (const auto& tx : block.transactions) {
        txArray.pushBack(JsonValue(tx.getHash().toHex()));
    }
    result.set("tx", txArray);

    return result;
}

JsonValue BlockchainRpc::blockHeaderToJson(const core::BlockHeader& header) const {
    JsonValue result = JsonValue::makeObject();
    result.set("hash", JsonValue(header.getHash().toHex()));
    result.set("version", JsonValue(static_cast<int64_t>(header.version)));
    result.set("previousblockhash", JsonValue(header.previousBlockHash.toHex()));
    result.set("merkleroot", JsonValue(header.merkleRoot.toHex()));
    result.set("time", JsonValue(static_cast<int64_t>(header.timestamp)));
    result.set("bits", JsonValue(header.bits));
    result.set("nonce", JsonValue(static_cast<int64_t>(header.nonce)));

    return result;
}

JsonValue BlockchainRpc::transactionToJson(const core::Transaction& tx, bool verbose) const {
    if (!verbose) {
        auto serialized = tx.serialize();
        std::string hex;
        for (uint8_t byte : serialized) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            hex += buf;
        }
        return JsonValue(hex);
    }

    JsonValue result = JsonValue::makeObject();
    result.set("txid", JsonValue(tx.getHash().toHex()));
    result.set("version", JsonValue(static_cast<int64_t>(tx.version)));
    result.set("locktime", JsonValue(static_cast<int64_t>(tx.lockTime)));

    JsonValue vinArray = JsonValue::makeArray();
    for (const auto& input : tx.inputs) {
        JsonValue vin = JsonValue::makeObject();
        vin.set("txid", JsonValue(input.previousOutput.txHash.toHex()));
        vin.set("vout", JsonValue(static_cast<int64_t>(input.previousOutput.vout)));
        vin.set("sequence", JsonValue(static_cast<int64_t>(input.sequence)));
        vinArray.pushBack(vin);
    }
    result.set("vin", vinArray);

    JsonValue voutArray = JsonValue::makeArray();
    for (const auto& output : tx.outputs) {
        JsonValue vout = JsonValue::makeObject();
        vout.set("value", JsonValue(static_cast<double>(output.value) / 100000000.0));
        vout.set("n", JsonValue(static_cast<int64_t>(0)));  // Would need index
        voutArray.pushBack(vout);
    }
    result.set("vout", voutArray);

    return result;
}

}  // namespace rpc
}  // namespace ubuntu
