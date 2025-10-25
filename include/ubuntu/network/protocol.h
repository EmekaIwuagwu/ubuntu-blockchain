#pragma once

#include "ubuntu/core/block.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/network/peer.h"
#include "network.pb.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ubuntu {
namespace network {

/**
 * @brief Network message types
 */
enum class MessageType {
    VERSION,
    VERACK,
    PING,
    PONG,
    ADDR,
    INV,
    GETDATA,
    GETBLOCKS,
    GETHEADERS,
    TX,
    BLOCK,
    HEADERS,
    GETADDR,
    MEMPOOL,
    REJECT,
    SENDHEADERS,
    FEEFILTER,
    SENDCMPCT,
    CMPCTBLOCK,
    GETBLOCKTXN,
    BLOCKTXN
};

/**
 * @brief Protocol message handler
 *
 * Processes incoming network messages and dispatches to appropriate handlers.
 */
class ProtocolHandler {
public:
    ProtocolHandler();
    ~ProtocolHandler();

    /**
     * @brief Process an incoming message
     *
     * @param peer Peer that sent the message
     * @param message Serialized network message
     * @return true if message was processed successfully
     */
    bool processMessage(Peer& peer, const std::vector<uint8_t>& message);

    /**
     * @brief Create a VERSION message
     *
     * @param addrRecv Address of receiving peer
     * @param startHeight Current blockchain height
     * @return Serialized VERSION message
     */
    std::vector<uint8_t> createVersionMessage(const NetAddress& addrRecv,
                                               uint32_t startHeight);

    /**
     * @brief Create a VERACK message
     *
     * @return Serialized VERACK message
     */
    std::vector<uint8_t> createVerackMessage();

    /**
     * @brief Create a PING message
     *
     * @param nonce Random nonce for latency measurement
     * @return Serialized PING message
     */
    std::vector<uint8_t> createPingMessage(uint64_t nonce);

    /**
     * @brief Create a PONG message
     *
     * @param nonce Nonce from PING message
     * @return Serialized PONG message
     */
    std::vector<uint8_t> createPongMessage(uint64_t nonce);

    /**
     * @brief Create an INV message
     *
     * @param invVectors Inventory vectors to announce
     * @return Serialized INV message
     */
    std::vector<uint8_t> createInvMessage(const std::vector<proto::InvVector>& invVectors);

    /**
     * @brief Create a GETDATA message
     *
     * @param invVectors Inventory vectors to request
     * @return Serialized GETDATA message
     */
    std::vector<uint8_t> createGetDataMessage(
        const std::vector<proto::InvVector>& invVectors);

    /**
     * @brief Create a TX message
     *
     * @param tx Transaction to broadcast
     * @return Serialized TX message
     */
    std::vector<uint8_t> createTxMessage(const core::Transaction& tx);

    /**
     * @brief Create a BLOCK message
     *
     * @param block Block to send
     * @return Serialized BLOCK message
     */
    std::vector<uint8_t> createBlockMessage(const core::Block& block);

    /**
     * @brief Create a GETHEADERS message
     *
     * @param locatorHashes Block locator hashes
     * @param stopHash Hash to stop at
     * @return Serialized GETHEADERS message
     */
    std::vector<uint8_t> createGetHeadersMessage(
        const std::vector<crypto::Hash256>& locatorHashes,
        const crypto::Hash256& stopHash);

    /**
     * @brief Create a HEADERS message
     *
     * @param headers Block headers to send
     * @return Serialized HEADERS message
     */
    std::vector<uint8_t> createHeadersMessage(
        const std::vector<core::BlockHeader>& headers);

    /**
     * @brief Set callback for VERSION messages
     */
    void setOnVersion(std::function<void(Peer&, const proto::VersionMessage&)> callback);

    /**
     * @brief Set callback for VERACK messages
     */
    void setOnVerack(std::function<void(Peer&)> callback);

    /**
     * @brief Set callback for PING messages
     */
    void setOnPing(std::function<void(Peer&, uint64_t)> callback);

    /**
     * @brief Set callback for PONG messages
     */
    void setOnPong(std::function<void(Peer&, uint64_t)> callback);

    /**
     * @brief Set callback for INV messages
     */
    void setOnInv(std::function<void(Peer&, const std::vector<proto::InvVector>&)> callback);

    /**
     * @brief Set callback for GETDATA messages
     */
    void setOnGetData(std::function<void(Peer&, const std::vector<proto::InvVector>&)> callback);

    /**
     * @brief Set callback for TX messages
     */
    void setOnTx(std::function<void(Peer&, const core::Transaction&)> callback);

    /**
     * @brief Set callback for BLOCK messages
     */
    void setOnBlock(std::function<void(Peer&, const core::Block&)> callback);

    /**
     * @brief Set callback for GETHEADERS messages
     */
    void setOnGetHeaders(std::function<void(Peer&, const std::vector<crypto::Hash256>&,
                                            const crypto::Hash256&)> callback);

    /**
     * @brief Set callback for HEADERS messages
     */
    void setOnHeaders(std::function<void(Peer&, const std::vector<core::BlockHeader>&)> callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Deserialize a network message
     */
    std::optional<proto::NetworkMessage> deserializeMessage(
        const std::vector<uint8_t>& data) const;

    /**
     * @brief Serialize a network message
     */
    std::vector<uint8_t> serializeMessage(const proto::NetworkMessage& message) const;

    /**
     * @brief Convert Transaction to protobuf
     */
    proto::Transaction transactionToProto(const core::Transaction& tx) const;

    /**
     * @brief Convert protobuf to Transaction
     */
    std::optional<core::Transaction> transactionFromProto(const proto::Transaction& tx) const;

    /**
     * @brief Convert Block to protobuf
     */
    proto::Block blockToProto(const core::Block& block) const;

    /**
     * @brief Convert protobuf to Block
     */
    std::optional<core::Block> blockFromProto(const proto::Block& block) const;

    /**
     * @brief Convert BlockHeader to protobuf
     */
    proto::BlockHeader headerToProto(const core::BlockHeader& header) const;

    /**
     * @brief Convert protobuf to BlockHeader
     */
    std::optional<core::BlockHeader> headerFromProto(const proto::BlockHeader& header) const;
};

}  // namespace network
}  // namespace ubuntu
