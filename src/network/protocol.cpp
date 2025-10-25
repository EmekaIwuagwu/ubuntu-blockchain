#include "ubuntu/network/protocol.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace ubuntu {
namespace network {

// ============================================================================
// ProtocolHandler::Impl
// ============================================================================

struct ProtocolHandler::Impl {
    // Message callbacks
    std::function<void(Peer&, const proto::VersionMessage&)> onVersion;
    std::function<void(Peer&)> onVerack;
    std::function<void(Peer&, uint64_t)> onPing;
    std::function<void(Peer&, uint64_t)> onPong;
    std::function<void(Peer&, const std::vector<proto::InvVector>&)> onInv;
    std::function<void(Peer&, const std::vector<proto::InvVector>&)> onGetData;
    std::function<void(Peer&, const core::Transaction&)> onTx;
    std::function<void(Peer&, const core::Block&)> onBlock;
    std::function<void(Peer&, const std::vector<crypto::Hash256>&, const crypto::Hash256&)>
        onGetHeaders;
    std::function<void(Peer&, const std::vector<core::BlockHeader>&)> onHeaders;

    // Protocol constants
    static constexpr uint32_t PROTOCOL_VERSION = 70015;
    static constexpr const char* USER_AGENT = "/UbuntuBlockchain:1.0.0/";
    static constexpr uint64_t NODE_NETWORK = (1 << 0);  // Full node
};

// ============================================================================
// ProtocolHandler Implementation
// ============================================================================

ProtocolHandler::ProtocolHandler() : impl_(std::make_unique<Impl>()) {}

ProtocolHandler::~ProtocolHandler() = default;

bool ProtocolHandler::processMessage(Peer& peer, const std::vector<uint8_t>& message) {
    auto netMsg = deserializeMessage(message);
    if (!netMsg) {
        spdlog::warn("Failed to deserialize message from {}", peer.getAddress().toString());
        peer.increaseBanScore(10);
        return false;
    }

    try {
        switch (static_cast<MessageType>(netMsg->type())) {
            case MessageType::VERSION:
                if (netMsg->has_version() && impl_->onVersion) {
                    impl_->onVersion(peer, netMsg->version());
                }
                break;

            case MessageType::VERACK:
                if (impl_->onVerack) {
                    impl_->onVerack(peer);
                }
                break;

            case MessageType::PING:
                if (netMsg->has_ping() && impl_->onPing) {
                    impl_->onPing(peer, netMsg->ping().nonce());
                }
                break;

            case MessageType::PONG:
                if (netMsg->has_pong() && impl_->onPong) {
                    impl_->onPong(peer, netMsg->pong().nonce());
                }
                break;

            case MessageType::INV:
                if (netMsg->has_inv() && impl_->onInv) {
                    std::vector<proto::InvVector> invVectors;
                    for (const auto& inv : netMsg->inv().inventory()) {
                        invVectors.push_back(inv);
                    }
                    impl_->onInv(peer, invVectors);
                }
                break;

            case MessageType::GETDATA:
                if (netMsg->has_getdata() && impl_->onGetData) {
                    std::vector<proto::InvVector> invVectors;
                    for (const auto& inv : netMsg->getdata().inventory()) {
                        invVectors.push_back(inv);
                    }
                    impl_->onGetData(peer, invVectors);
                }
                break;

            case MessageType::TX:
                if (netMsg->has_tx() && impl_->onTx) {
                    auto tx = transactionFromProto(netMsg->tx());
                    if (tx) {
                        impl_->onTx(peer, *tx);
                    }
                }
                break;

            case MessageType::BLOCK:
                if (netMsg->has_block() && impl_->onBlock) {
                    auto block = blockFromProto(netMsg->block());
                    if (block) {
                        impl_->onBlock(peer, *block);
                    }
                }
                break;

            case MessageType::GETHEADERS:
                if (netMsg->has_getheaders() && impl_->onGetHeaders) {
                    std::vector<crypto::Hash256> locators;
                    for (const auto& hash : netMsg->getheaders().locator_hashes()) {
                        locators.emplace_back(
                            std::vector<uint8_t>(hash.begin(), hash.end()));
                    }
                    crypto::Hash256 stopHash(std::vector<uint8_t>(
                        netMsg->getheaders().hash_stop().begin(),
                        netMsg->getheaders().hash_stop().end()));
                    impl_->onGetHeaders(peer, locators, stopHash);
                }
                break;

            case MessageType::HEADERS:
                if (netMsg->has_headers() && impl_->onHeaders) {
                    std::vector<core::BlockHeader> headers;
                    for (const auto& protoHeader : netMsg->headers().headers()) {
                        auto header = headerFromProto(protoHeader);
                        if (header) {
                            headers.push_back(*header);
                        }
                    }
                    impl_->onHeaders(peer, headers);
                }
                break;

            default:
                spdlog::debug("Unhandled message type: {}", netMsg->type());
                break;
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error processing message from {}: {}", peer.getAddress().toString(),
                      e.what());
        peer.increaseBanScore(20);
        return false;
    }
}

std::vector<uint8_t> ProtocolHandler::createVersionMessage(const NetAddress& addrRecv,
                                                            uint32_t startHeight) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::VERSION));

    auto* version = netMsg.mutable_version();
    version->set_protocol_version(Impl::PROTOCOL_VERSION);
    version->set_services(Impl::NODE_NETWORK);
    version->set_timestamp(
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000);

    auto* addrRecvProto = version->mutable_addr_recv();
    addrRecvProto->set_services(addrRecv.services);
    addrRecvProto->set_ip(addrRecv.ip);
    addrRecvProto->set_port(addrRecv.port);

    version->set_user_agent(Impl::USER_AGENT);
    version->set_start_height(startHeight);

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createVerackMessage() {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::VERACK));
    netMsg.mutable_verack();
    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createPingMessage(uint64_t nonce) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::PING));

    auto* ping = netMsg.mutable_ping();
    ping->set_nonce(nonce);

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createPongMessage(uint64_t nonce) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::PONG));

    auto* pong = netMsg.mutable_pong();
    pong->set_nonce(nonce);

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createInvMessage(
    const std::vector<proto::InvVector>& invVectors) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::INV));

    auto* inv = netMsg.mutable_inv();
    for (const auto& invVec : invVectors) {
        *inv->add_inventory() = invVec;
    }

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createGetDataMessage(
    const std::vector<proto::InvVector>& invVectors) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::GETDATA));

    auto* getData = netMsg.mutable_getdata();
    for (const auto& invVec : invVectors) {
        *getData->add_inventory() = invVec;
    }

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createTxMessage(const core::Transaction& tx) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::TX));

    *netMsg.mutable_tx() = transactionToProto(tx);

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createBlockMessage(const core::Block& block) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::BLOCK));

    *netMsg.mutable_block() = blockToProto(block);

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createGetHeadersMessage(
    const std::vector<crypto::Hash256>& locatorHashes,
    const crypto::Hash256& stopHash) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::GETHEADERS));

    auto* getHeaders = netMsg.mutable_getheaders();
    getHeaders->set_protocol_version(Impl::PROTOCOL_VERSION);

    for (const auto& hash : locatorHashes) {
        getHeaders->add_locator_hashes(hash.data(), hash.size());
    }

    getHeaders->set_hash_stop(stopHash.data(), stopHash.size());

    return serializeMessage(netMsg);
}

std::vector<uint8_t> ProtocolHandler::createHeadersMessage(
    const std::vector<core::BlockHeader>& headers) {
    proto::NetworkMessage netMsg;
    netMsg.set_type(static_cast<uint32_t>(MessageType::HEADERS));

    auto* headersMsg = netMsg.mutable_headers();
    for (const auto& header : headers) {
        *headersMsg->add_headers() = headerToProto(header);
    }

    return serializeMessage(netMsg);
}

void ProtocolHandler::setOnVersion(
    std::function<void(Peer&, const proto::VersionMessage&)> callback) {
    impl_->onVersion = callback;
}

void ProtocolHandler::setOnVerack(std::function<void(Peer&)> callback) {
    impl_->onVerack = callback;
}

void ProtocolHandler::setOnPing(std::function<void(Peer&, uint64_t)> callback) {
    impl_->onPing = callback;
}

void ProtocolHandler::setOnPong(std::function<void(Peer&, uint64_t)> callback) {
    impl_->onPong = callback;
}

void ProtocolHandler::setOnInv(
    std::function<void(Peer&, const std::vector<proto::InvVector>&)> callback) {
    impl_->onInv = callback;
}

void ProtocolHandler::setOnGetData(
    std::function<void(Peer&, const std::vector<proto::InvVector>&)> callback) {
    impl_->onGetData = callback;
}

void ProtocolHandler::setOnTx(std::function<void(Peer&, const core::Transaction&)> callback) {
    impl_->onTx = callback;
}

void ProtocolHandler::setOnBlock(std::function<void(Peer&, const core::Block&)> callback) {
    impl_->onBlock = callback;
}

void ProtocolHandler::setOnGetHeaders(
    std::function<void(Peer&, const std::vector<crypto::Hash256>&, const crypto::Hash256&)>
        callback) {
    impl_->onGetHeaders = callback;
}

void ProtocolHandler::setOnHeaders(
    std::function<void(Peer&, const std::vector<core::BlockHeader>&)> callback) {
    impl_->onHeaders = callback;
}

std::optional<proto::NetworkMessage> ProtocolHandler::deserializeMessage(
    const std::vector<uint8_t>& data) const {
    proto::NetworkMessage message;
    if (message.ParseFromArray(data.data(), data.size())) {
        return message;
    }
    return std::nullopt;
}

std::vector<uint8_t> ProtocolHandler::serializeMessage(
    const proto::NetworkMessage& message) const {
    std::vector<uint8_t> data(message.ByteSizeLong());
    message.SerializeToArray(data.data(), data.size());
    return data;
}

proto::Transaction ProtocolHandler::transactionToProto(const core::Transaction& tx) const {
    proto::Transaction protoTx;
    protoTx.set_version(tx.version);
    protoTx.set_locktime(tx.lockTime);

    for (const auto& input : tx.inputs) {
        auto* protoInput = protoTx.add_inputs();
        protoInput->set_previous_tx_hash(input.previousOutput.txHash.data(),
                                         input.previousOutput.txHash.size());
        protoInput->set_previous_output_index(input.previousOutput.vout);
        protoInput->set_script_sig(input.scriptSig.data(), input.scriptSig.size());
        protoInput->set_sequence(input.sequence);
    }

    for (const auto& output : tx.outputs) {
        auto* protoOutput = protoTx.add_outputs();
        protoOutput->set_value(output.value);
        protoOutput->set_script_pub_key(output.scriptPubKey.data(),
                                        output.scriptPubKey.size());
    }

    return protoTx;
}

std::optional<core::Transaction> ProtocolHandler::transactionFromProto(
    const proto::Transaction& protoTx) const {
    core::Transaction tx;
    tx.version = protoTx.version();
    tx.lockTime = protoTx.locktime();

    for (const auto& protoInput : protoTx.inputs()) {
        core::TxInput input;
        input.previousOutput.txHash = crypto::Hash256(std::vector<uint8_t>(
            protoInput.previous_tx_hash().begin(), protoInput.previous_tx_hash().end()));
        input.previousOutput.vout = protoInput.previous_output_index();
        input.scriptSig = std::vector<uint8_t>(protoInput.script_sig().begin(),
                                               protoInput.script_sig().end());
        input.sequence = protoInput.sequence();
        tx.inputs.push_back(input);
    }

    for (const auto& protoOutput : protoTx.outputs()) {
        core::TxOutput output;
        output.value = protoOutput.value();
        output.scriptPubKey = std::vector<uint8_t>(protoOutput.script_pub_key().begin(),
                                                    protoOutput.script_pub_key().end());
        tx.outputs.push_back(output);
    }

    return tx;
}

proto::Block ProtocolHandler::blockToProto(const core::Block& block) const {
    proto::Block protoBlock;

    *protoBlock.mutable_header() = headerToProto(block.header);

    for (const auto& tx : block.transactions) {
        *protoBlock.add_transactions() = transactionToProto(tx);
    }

    return protoBlock;
}

std::optional<core::Block> ProtocolHandler::blockFromProto(const proto::Block& protoBlock) const {
    core::Block block;

    auto header = headerFromProto(protoBlock.header());
    if (!header) {
        return std::nullopt;
    }
    block.header = *header;

    for (const auto& protoTx : protoBlock.transactions()) {
        auto tx = transactionFromProto(protoTx);
        if (!tx) {
            return std::nullopt;
        }
        block.transactions.push_back(*tx);
    }

    return block;
}

proto::BlockHeader ProtocolHandler::headerToProto(const core::BlockHeader& header) const {
    proto::BlockHeader protoHeader;
    protoHeader.set_version(header.version);
    protoHeader.set_previous_block_hash(header.previousBlockHash.data(),
                                        header.previousBlockHash.size());
    protoHeader.set_merkle_root(header.merkleRoot.data(), header.merkleRoot.size());
    protoHeader.set_timestamp(header.timestamp);
    protoHeader.set_bits(header.bits);
    protoHeader.set_nonce(header.nonce);
    return protoHeader;
}

std::optional<core::BlockHeader> ProtocolHandler::headerFromProto(
    const proto::BlockHeader& protoHeader) const {
    core::BlockHeader header;
    header.version = protoHeader.version();
    header.previousBlockHash = crypto::Hash256(std::vector<uint8_t>(
        protoHeader.previous_block_hash().begin(), protoHeader.previous_block_hash().end()));
    header.merkleRoot = crypto::Hash256(std::vector<uint8_t>(
        protoHeader.merkle_root().begin(), protoHeader.merkle_root().end()));
    header.timestamp = protoHeader.timestamp();
    header.bits = protoHeader.bits();
    header.nonce = protoHeader.nonce();
    return header;
}

}  // namespace network
}  // namespace ubuntu
