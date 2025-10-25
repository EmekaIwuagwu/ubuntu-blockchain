#pragma once

#include "ubuntu/consensus/chainparams.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/mempool/mempool.h"
#include "ubuntu/network/peer.h"
#include "ubuntu/network/protocol.h"
#include "ubuntu/storage/block_index.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ubuntu {
namespace network {

/**
 * @brief Network manager for P2P communication
 *
 * Coordinates peer connections, block propagation, and transaction broadcasting.
 */
class NetworkManager {
public:
    /**
     * @brief Construct network manager
     *
     * @param blockchain Blockchain instance
     * @param mempool Transaction mempool
     * @param blockStorage Block storage
     * @param params Chain parameters
     */
    NetworkManager(std::shared_ptr<core::Blockchain> blockchain,
                   std::shared_ptr<mempool::Mempool> mempool,
                   std::shared_ptr<storage::BlockStorage> blockStorage,
                   const consensus::ChainParams& params);

    ~NetworkManager();

    /**
     * @brief Start network services
     *
     * @param listenPort Port to listen on
     * @return true if started successfully
     */
    bool start(uint16_t listenPort = 8333);

    /**
     * @brief Stop network services
     */
    void stop();

    /**
     * @brief Check if network is running
     *
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Connect to a peer
     *
     * @param address Peer address
     * @return true if connection initiated
     */
    bool connectToPeer(const NetAddress& address);

    /**
     * @brief Disconnect from a peer
     *
     * @param address Peer address
     */
    void disconnectPeer(const NetAddress& address);

    /**
     * @brief Broadcast a transaction to all peers
     *
     * @param tx Transaction to broadcast
     */
    void broadcastTransaction(const core::Transaction& tx);

    /**
     * @brief Broadcast a block to all peers
     *
     * @param block Block to broadcast
     */
    void broadcastBlock(const core::Block& block);

    /**
     * @brief Get number of connected peers
     *
     * @return Peer count
     */
    size_t getPeerCount() const;

    /**
     * @brief Get list of connected peers
     *
     * @return Vector of peer addresses
     */
    std::vector<NetAddress> getPeers() const;

    /**
     * @brief Add seed nodes for initial peer discovery
     *
     * @param seeds Vector of seed node addresses
     */
    void addSeedNodes(const std::vector<NetAddress>& seeds);

    /**
     * @brief Get network statistics
     */
    struct NetworkStats {
        size_t connectedPeers;
        size_t totalConnections;
        size_t blocksSent;
        size_t blocksReceived;
        size_t txSent;
        size_t txReceived;
        uint64_t bytesSent;
        uint64_t bytesReceived;
    };

    NetworkStats getStats() const;

private:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<mempool::Mempool> mempool_;
    std::shared_ptr<storage::BlockStorage> blockStorage_;
    consensus::ChainParams params_;

    std::unique_ptr<ProtocolHandler> protocol_;
    std::unique_ptr<PeerDatabase> peerDb_;

    // Peer management
    mutable std::mutex peersMutex_;
    std::unordered_map<std::string, std::unique_ptr<Peer>> peers_;

    // Network threads
    std::atomic<bool> running_;
    std::vector<std::thread> threads_;

    // Statistics
    mutable std::mutex statsMutex_;
    NetworkStats stats_;

    // Configuration
    uint16_t listenPort_;
    size_t maxConnections_ = 125;
    size_t maxOutboundConnections_ = 8;

    /**
     * @brief Main network loop
     */
    void networkLoop();

    /**
     * @brief Accept incoming connections
     */
    void acceptLoop();

    /**
     * @brief Peer discovery loop
     */
    void discoveryLoop();

    /**
     * @brief Handle VERSION message
     */
    void handleVersion(Peer& peer, const proto::VersionMessage& version);

    /**
     * @brief Handle VERACK message
     */
    void handleVerack(Peer& peer);

    /**
     * @brief Handle PING message
     */
    void handlePing(Peer& peer, uint64_t nonce);

    /**
     * @brief Handle PONG message
     */
    void handlePong(Peer& peer, uint64_t nonce);

    /**
     * @brief Handle INV message
     */
    void handleInv(Peer& peer, const std::vector<proto::InvVector>& inv);

    /**
     * @brief Handle GETDATA message
     */
    void handleGetData(Peer& peer, const std::vector<proto::InvVector>& inv);

    /**
     * @brief Handle TX message
     */
    void handleTx(Peer& peer, const core::Transaction& tx);

    /**
     * @brief Handle BLOCK message
     */
    void handleBlock(Peer& peer, const core::Block& block);

    /**
     * @brief Handle GETHEADERS message
     */
    void handleGetHeaders(Peer& peer,
                          const std::vector<crypto::Hash256>& locators,
                          const crypto::Hash256& stopHash);

    /**
     * @brief Handle HEADERS message
     */
    void handleHeaders(Peer& peer, const std::vector<core::BlockHeader>& headers);

    /**
     * @brief Perform handshake with peer
     */
    bool performHandshake(Peer& peer);

    /**
     * @brief Send VERSION message
     */
    void sendVersion(Peer& peer);

    /**
     * @brief Send VERACK message
     */
    void sendVerack(Peer& peer);

    /**
     * @brief Request headers from peer
     */
    void requestHeaders(Peer& peer);

    /**
     * @brief Request blocks from peer
     */
    void requestBlocks(Peer& peer, const std::vector<crypto::Hash256>& hashes);

    /**
     * @brief Get block locator hashes
     */
    std::vector<crypto::Hash256> getBlockLocator() const;

    /**
     * @brief Cleanup disconnected peers
     */
    void cleanupPeers();

    /**
     * @brief Get random peers for connection
     */
    std::vector<NetAddress> getRandomPeers(size_t count) const;
};

}  // namespace network
}  // namespace ubuntu
