#include "ubuntu/network/network_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <random>

namespace ubuntu {
namespace network {

// ============================================================================
// NetworkManager Implementation
// ============================================================================

NetworkManager::NetworkManager(std::shared_ptr<core::Blockchain> blockchain,
                               std::shared_ptr<mempool::Mempool> mempool,
                               std::shared_ptr<storage::BlockStorage> blockStorage,
                               const consensus::ChainParams& params)
    : blockchain_(blockchain),
      mempool_(mempool),
      blockStorage_(blockStorage),
      params_(params),
      protocol_(std::make_unique<ProtocolHandler>()),
      peerDb_(std::make_unique<PeerDatabase>()),
      running_(false),
      listenPort_(8333) {

    // Set up protocol handlers
    protocol_->setOnVersion([this](Peer& peer, const proto::VersionMessage& version) {
        handleVersion(peer, version);
    });

    protocol_->setOnVerack([this](Peer& peer) { handleVerack(peer); });

    protocol_->setOnPing([this](Peer& peer, uint64_t nonce) { handlePing(peer, nonce); });

    protocol_->setOnPong([this](Peer& peer, uint64_t nonce) { handlePong(peer, nonce); });

    protocol_->setOnInv([this](Peer& peer, const std::vector<proto::InvVector>& inv) {
        handleInv(peer, inv);
    });

    protocol_->setOnGetData([this](Peer& peer, const std::vector<proto::InvVector>& inv) {
        handleGetData(peer, inv);
    });

    protocol_->setOnTx([this](Peer& peer, const core::Transaction& tx) { handleTx(peer, tx); });

    protocol_->setOnBlock([this](Peer& peer, const core::Block& block) {
        handleBlock(peer, block);
    });

    protocol_->setOnGetHeaders(
        [this](Peer& peer, const std::vector<crypto::Hash256>& locators,
               const crypto::Hash256& stopHash) { handleGetHeaders(peer, locators, stopHash); });

    protocol_->setOnHeaders(
        [this](Peer& peer, const std::vector<core::BlockHeader>& headers) {
            handleHeaders(peer, headers);
        });

    // Initialize statistics
    stats_ = {};
}

NetworkManager::~NetworkManager() {
    stop();
}

bool NetworkManager::start(uint16_t listenPort) {
    if (running_) {
        spdlog::warn("Network manager already running");
        return true;
    }

    listenPort_ = listenPort;
    running_ = true;

    // Start network threads
    threads_.emplace_back(&NetworkManager::networkLoop, this);
    threads_.emplace_back(&NetworkManager::discoveryLoop, this);
    threads_.emplace_back(&NetworkManager::acceptLoop, this);

    spdlog::info("Network manager started on port {}", listenPort_);
    return true;
}

void NetworkManager::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping network manager...");
    running_ = false;

    // Wait for threads to finish
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();

    // Disconnect all peers
    std::lock_guard<std::mutex> lock(peersMutex_);
    peers_.clear();

    spdlog::info("Network manager stopped");
}

bool NetworkManager::isRunning() const {
    return running_;
}

bool NetworkManager::connectToPeer(const NetAddress& address) {
    std::lock_guard<std::mutex> lock(peersMutex_);

    // Check if already connected
    std::string key = address.toString();
    if (peers_.find(key) != peers_.end()) {
        spdlog::debug("Already connected to {}", key);
        return false;
    }

    // Check max connections
    if (peers_.size() >= maxConnections_) {
        spdlog::debug("Max connections reached");
        return false;
    }

    // Create and connect peer
    auto peer = std::make_unique<Peer>(address);
    if (!peer->connect()) {
        spdlog::warn("Failed to connect to {}", key);
        return false;
    }

    // Perform handshake
    if (!performHandshake(*peer)) {
        spdlog::warn("Handshake failed with {}", key);
        return false;
    }

    spdlog::info("Connected to peer: {}", key);
    peers_[key] = std::move(peer);

    return true;
}

void NetworkManager::disconnectPeer(const NetAddress& address) {
    std::lock_guard<std::mutex> lock(peersMutex_);

    std::string key = address.toString();
    auto it = peers_.find(key);
    if (it != peers_.end()) {
        spdlog::info("Disconnecting from {}", key);
        peers_.erase(it);
    }
}

void NetworkManager::broadcastTransaction(const core::Transaction& tx) {
    auto txHash = tx.getHash();
    spdlog::info("Broadcasting transaction {}", txHash.toHex());

    proto::InvVector inv;
    inv.set_type(proto::InvVector::TX);
    inv.set_hash(txHash.data(), txHash.size());

    std::vector<proto::InvVector> invVectors = {inv};
    auto invMsg = protocol_->createInvMessage(invVectors);

    std::lock_guard<std::mutex> lock(peersMutex_);
    for (const auto& [key, peer] : peers_) {
        if (peer->getState() == PeerState::READY) {
            peer->send(invMsg);
        }
    }

    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.txSent++;
    stats_.bytesSent += invMsg.size();
}

void NetworkManager::broadcastBlock(const core::Block& block) {
    auto blockHash = block.getHash();
    spdlog::info("Broadcasting block {}", blockHash.toHex());

    proto::InvVector inv;
    inv.set_type(proto::InvVector::BLOCK);
    inv.set_hash(blockHash.data(), blockHash.size());

    std::vector<proto::InvVector> invVectors = {inv};
    auto invMsg = protocol_->createInvMessage(invVectors);

    std::lock_guard<std::mutex> lock(peersMutex_);
    for (const auto& [key, peer] : peers_) {
        if (peer->getState() == PeerState::READY) {
            peer->send(invMsg);
        }
    }

    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.blocksSent++;
    stats_.bytesSent += invMsg.size();
}

size_t NetworkManager::getPeerCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peers_.size();
}

std::vector<NetAddress> NetworkManager::getPeers() const {
    std::lock_guard<std::mutex> lock(peersMutex_);

    std::vector<NetAddress> addresses;
    addresses.reserve(peers_.size());

    for (const auto& [key, peer] : peers_) {
        addresses.push_back(peer->getAddress());
    }

    return addresses;
}

void NetworkManager::addSeedNodes(const std::vector<NetAddress>& seeds) {
    for (const auto& seed : seeds) {
        peerDb_->addAddress(seed);
    }
    spdlog::info("Added {} seed nodes", seeds.size());
}

NetworkManager::NetworkStats NetworkManager::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void NetworkManager::networkLoop() {
    spdlog::info("Network loop started");

    while (running_) {
        // Cleanup disconnected peers
        cleanupPeers();

        // Process messages from all peers
        std::vector<Peer*> activePeers;
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            for (const auto& [key, peer] : peers_) {
                if (peer->getState() == PeerState::READY) {
                    activePeers.push_back(peer.get());
                }
            }
        }

        for (auto* peer : activePeers) {
            auto messages = peer->receiveMessages();
            for (const auto& message : messages) {
                protocol_->processMessage(*peer, message);

                std::lock_guard<std::mutex> statsLock(statsMutex_);
                stats_.bytesReceived += message.size();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Network loop stopped");
}

void NetworkManager::acceptLoop() {
    spdlog::info("Accept loop started on port {}", listenPort_);

    // In a full implementation, this would listen for incoming connections
    // using Boost.Asio or similar networking library

    while (running_) {
        // TODO: Accept incoming connections
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    spdlog::info("Accept loop stopped");
}

void NetworkManager::discoveryLoop() {
    spdlog::info("Discovery loop started");

    while (running_) {
        // Check if we need more connections
        size_t currentPeers = getPeerCount();
        if (currentPeers < maxOutboundConnections_) {
            size_t needed = maxOutboundConnections_ - currentPeers;
            auto candidates = getRandomPeers(needed);

            for (const auto& addr : candidates) {
                connectToPeer(addr);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    spdlog::info("Discovery loop stopped");
}

void NetworkManager::handleVersion(Peer& peer, const proto::VersionMessage& version) {
    spdlog::info("Received VERSION from {} (version: {}, height: {})",
                 peer.getAddress().toString(), version.protocol_version(),
                 version.start_height());

    // Update peer info
    peer.setVersion(version.protocol_version());
    peer.setStartHeight(version.start_height());

    // Send VERACK
    sendVerack(peer);

    // If we haven't sent our VERSION yet, send it now
    if (peer.getState() == PeerState::HANDSHAKING) {
        sendVersion(peer);
    }
}

void NetworkManager::handleVerack(Peer& peer) {
    spdlog::info("Received VERACK from {}", peer.getAddress().toString());

    peer.setState(PeerState::READY);

    // Request headers to sync
    requestHeaders(peer);

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.totalConnections++;
}

void NetworkManager::handlePing(Peer& peer, uint64_t nonce) {
    spdlog::debug("Received PING from {}", peer.getAddress().toString());

    // Send PONG with same nonce
    auto pongMsg = protocol_->createPongMessage(nonce);
    peer.send(pongMsg);
}

void NetworkManager::handlePong(Peer& peer, uint64_t nonce) {
    spdlog::debug("Received PONG from {}", peer.getAddress().toString());

    // Update peer latency
    peer.updateLatency();
}

void NetworkManager::handleInv(Peer& peer, const std::vector<proto::InvVector>& inv) {
    spdlog::debug("Received INV with {} items from {}", inv.size(),
                  peer.getAddress().toString());

    std::vector<proto::InvVector> toRequest;

    for (const auto& item : inv) {
        crypto::Hash256 hash(std::vector<uint8_t>(item.hash().begin(), item.hash().end()));

        if (item.type() == proto::InvVector::TX) {
            // Check if we have this transaction
            if (!mempool_->hasTransaction(hash)) {
                toRequest.push_back(item);
            }
        } else if (item.type() == proto::InvVector::BLOCK) {
            // Check if we have this block
            if (!blockStorage_->hasBlock(hash)) {
                toRequest.push_back(item);
            }
        }
    }

    // Request items we don't have
    if (!toRequest.empty()) {
        spdlog::debug("Requesting {} items from {}", toRequest.size(),
                      peer.getAddress().toString());
        auto getDataMsg = protocol_->createGetDataMessage(toRequest);
        peer.send(getDataMsg);
    }
}

void NetworkManager::handleGetData(Peer& peer, const std::vector<proto::InvVector>& inv) {
    spdlog::debug("Received GETDATA with {} items from {}", inv.size(),
                  peer.getAddress().toString());

    for (const auto& item : inv) {
        crypto::Hash256 hash(std::vector<uint8_t>(item.hash().begin(), item.hash().end()));

        if (item.type() == proto::InvVector::TX) {
            // Send transaction if we have it
            auto tx = mempool_->getTransaction(hash);
            if (tx) {
                auto txMsg = protocol_->createTxMessage(*tx);
                peer.send(txMsg);

                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.txSent++;
                stats_.bytesSent += txMsg.size();
            }
        } else if (item.type() == proto::InvVector::BLOCK) {
            // Send block if we have it
            auto block = blockStorage_->getBlock(hash);
            if (block) {
                auto blockMsg = protocol_->createBlockMessage(*block);
                peer.send(blockMsg);

                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.blocksSent++;
                stats_.bytesSent += blockMsg.size();
            }
        }
    }
}

void NetworkManager::handleTx(Peer& peer, const core::Transaction& tx) {
    auto txHash = tx.getHash();
    spdlog::info("Received transaction {} from {}", txHash.toHex(),
                 peer.getAddress().toString());

    // Add to mempool
    // In a full implementation, we would calculate the fee here
    uint64_t fee = 0;  // TODO: Calculate fee
    if (mempool_->addTransaction(tx, fee)) {
        spdlog::debug("Added transaction {} to mempool", txHash.toHex());

        // Relay to other peers
        broadcastTransaction(tx);
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.txReceived++;
}

void NetworkManager::handleBlock(Peer& peer, const core::Block& block) {
    auto blockHash = block.getHash();
    spdlog::info("Received block {} from {}", blockHash.toHex(), peer.getAddress().toString());

    // Add to blockchain
    if (blockchain_->addBlock(block)) {
        spdlog::info("Added block {} to blockchain", blockHash.toHex());

        // Remove conflicting transactions from mempool
        mempool_->removeConflictingTransactions(block);

        // Relay to other peers
        broadcastBlock(block);
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.blocksReceived++;
}

void NetworkManager::handleGetHeaders(Peer& peer,
                                       const std::vector<crypto::Hash256>& locators,
                                       const crypto::Hash256& stopHash) {
    spdlog::debug("Received GETHEADERS from {}", peer.getAddress().toString());

    // Find common ancestor using locator
    // In a full implementation, we would traverse the blockchain
    // to find the fork point and send headers

    // For now, send empty headers response
    std::vector<core::BlockHeader> headers;
    auto headersMsg = protocol_->createHeadersMessage(headers);
    peer.send(headersMsg);
}

void NetworkManager::handleHeaders(Peer& peer,
                                    const std::vector<core::BlockHeader>& headers) {
    spdlog::info("Received {} headers from {}", headers.size(), peer.getAddress().toString());

    // Request full blocks for headers we don't have
    std::vector<crypto::Hash256> toRequest;

    for (const auto& header : headers) {
        auto hash = header.getHash();
        if (!blockStorage_->hasBlock(hash)) {
            toRequest.push_back(hash);
        }
    }

    if (!toRequest.empty()) {
        requestBlocks(peer, toRequest);
    }
}

bool NetworkManager::performHandshake(Peer& peer) {
    peer.setState(PeerState::HANDSHAKING);

    // Send VERSION message
    sendVersion(peer);

    // In a full implementation, we would wait for VERSION and VERACK
    // For now, we'll simulate successful handshake
    // The actual handshake is completed when we receive VERACK in handleVerack()

    return true;
}

void NetworkManager::sendVersion(Peer& peer) {
    auto height = blockchain_->getState().height;
    auto versionMsg = protocol_->createVersionMessage(peer.getAddress(), height);
    peer.send(versionMsg);

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.bytesSent += versionMsg.size();
}

void NetworkManager::sendVerack(Peer& peer) {
    auto verackMsg = protocol_->createVerackMessage();
    peer.send(verackMsg);

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.bytesSent += verackMsg.size();
}

void NetworkManager::requestHeaders(Peer& peer) {
    auto locators = getBlockLocator();
    crypto::Hash256 stopHash;  // Zero hash means "send up to 2000 headers"

    auto getHeadersMsg = protocol_->createGetHeadersMessage(locators, stopHash);
    peer.send(getHeadersMsg);
}

void NetworkManager::requestBlocks(Peer& peer, const std::vector<crypto::Hash256>& hashes) {
    std::vector<proto::InvVector> invVectors;
    for (const auto& hash : hashes) {
        proto::InvVector inv;
        inv.set_type(proto::InvVector::BLOCK);
        inv.set_hash(hash.data(), hash.size());
        invVectors.push_back(inv);
    }

    auto getDataMsg = protocol_->createGetDataMessage(invVectors);
    peer.send(getDataMsg);
}

std::vector<crypto::Hash256> NetworkManager::getBlockLocator() const {
    std::vector<crypto::Hash256> locator;

    // Get current tip
    auto tip = blockchain_->getTip();
    if (!tip) {
        return locator;
    }

    auto height = blockchain_->getState().height;

    // Add hashes with exponentially increasing gaps
    // This allows efficient fork point discovery
    uint32_t step = 1;
    for (uint32_t h = height; h > 0 && locator.size() < 10; h -= step) {
        auto block = blockStorage_->getBlockByHeight(h);
        if (block) {
            locator.push_back(block->getHash());
        }

        // Exponential backoff
        if (locator.size() >= 10) {
            step *= 2;
        }
    }

    // Always include genesis
    auto genesis = blockStorage_->getBlockByHeight(0);
    if (genesis) {
        locator.push_back(genesis->getHash());
    }

    return locator;
}

void NetworkManager::cleanupPeers() {
    std::lock_guard<std::mutex> lock(peersMutex_);

    std::vector<std::string> toRemove;

    for (const auto& [key, peer] : peers_) {
        if (peer->getState() == PeerState::DISCONNECTED ||
            peer->getState() == PeerState::BANNED) {
            toRemove.push_back(key);
        }
    }

    for (const auto& key : toRemove) {
        spdlog::debug("Removing peer {}", key);
        peers_.erase(key);
    }
}

std::vector<NetAddress> NetworkManager::getRandomPeers(size_t count) const {
    return peerDb_->getRandomAddresses(count);
}

}  // namespace network
}  // namespace ubuntu
