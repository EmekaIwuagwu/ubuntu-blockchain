#pragma once

#include "ubuntu/crypto/hash.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ubuntu {
namespace network {

/**
 * @brief Peer connection state
 */
enum class PeerState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    HANDSHAKING,
    READY,
    BANNED
};

/**
 * @brief Service flags (what services a peer provides)
 */
enum ServiceFlags : uint64_t {
    NODE_NONE = 0,
    NODE_NETWORK = (1 << 0),      // Full node (can serve full blocks)
    NODE_BLOOM = (1 << 2),         // Bloom filter support
    NODE_WITNESS = (1 << 3),       // Segwit support
    NODE_COMPACT_FILTERS = (1 << 6), // Compact block filters
    NODE_NETWORK_LIMITED = (1 << 10) // Pruned node with recent blocks
};

/**
 * @brief Network address information
 */
struct NetAddress {
    std::string ip;
    uint16_t port;
    uint64_t services;
    std::chrono::system_clock::time_point lastSeen;

    NetAddress() : port(0), services(0) {}

    NetAddress(const std::string& ip_, uint16_t port_, uint64_t services_ = NODE_NETWORK)
        : ip(ip_), port(port_), services(services_),
          lastSeen(std::chrono::system_clock::now()) {}

    std::string toString() const;
    bool operator==(const NetAddress& other) const;
    bool operator<(const NetAddress& other) const;
};

/**
 * @brief Peer connection information
 */
class Peer {
public:
    explicit Peer(const NetAddress& address);
    ~Peer();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const { return state_ == PeerState::CONNECTED || state_ == PeerState::READY; }

    // State
    PeerState getState() const { return state_; }
    void setState(PeerState state) { state_ = state; }

    // Network info
    const NetAddress& getAddress() const { return address_; }
    uint64_t getServices() const { return services_; }
    void setServices(uint64_t services) { services_ = services; }

    // Protocol info
    uint32_t getProtocolVersion() const { return protocolVersion_; }
    void setProtocolVersion(uint32_t version) { protocolVersion_ = version; }

    std::string getUserAgent() const { return userAgent_; }
    void setUserAgent(const std::string& agent) { userAgent_ = agent; }

    uint32_t getStartHeight() const { return startHeight_; }
    void setStartHeight(uint32_t height) { startHeight_ = height; }

    // Connection metadata
    uint64_t getNonce() const { return nonce_; }
    void setNonce(uint64_t nonce) { nonce_ = nonce; }

    bool isInbound() const { return inbound_; }
    void setInbound(bool inbound) { inbound_ = inbound; }

    // Statistics
    uint64_t getBytesSent() const { return bytesSent_; }
    uint64_t getBytesReceived() const { return bytesReceived_; }
    void addBytesSent(uint64_t bytes) { bytesSent_ += bytes; }
    void addBytesReceived(uint64_t bytes) { bytesReceived_ += bytes; }

    std::chrono::system_clock::time_point getConnectedTime() const { return connectedTime_; }
    std::chrono::system_clock::time_point getLastMessageTime() const { return lastMessageTime_; }
    void updateLastMessageTime() { lastMessageTime_ = std::chrono::system_clock::now(); }

    // Ban management
    uint32_t getBanScore() const { return banScore_; }
    void increaseBanScore(uint32_t points);
    bool isBanned() const { return state_ == PeerState::BANNED; }

    // Ping/latency
    void recordPing();
    void recordPong();
    int64_t getLatencyMs() const { return latencyMs_; }

private:
    NetAddress address_;
    PeerState state_;
    uint64_t services_;

    // Protocol
    uint32_t protocolVersion_;
    std::string userAgent_;
    uint32_t startHeight_;
    uint64_t nonce_;
    bool inbound_;

    // Statistics
    uint64_t bytesSent_;
    uint64_t bytesReceived_;
    std::chrono::system_clock::time_point connectedTime_;
    std::chrono::system_clock::time_point lastMessageTime_;

    // Ban tracking
    uint32_t banScore_;
    static constexpr uint32_t BAN_THRESHOLD = 100;

    // Latency tracking
    std::chrono::system_clock::time_point lastPingTime_;
    int64_t latencyMs_;
};

/**
 * @brief Peer database for managing known peers
 */
class PeerDatabase {
public:
    PeerDatabase();
    ~PeerDatabase();

    /**
     * @brief Add a peer address
     */
    void addAddress(const NetAddress& address);

    /**
     * @brief Get random peer addresses
     *
     * @param count Maximum number of addresses to return
     * @return Vector of peer addresses
     */
    std::vector<NetAddress> getRandomAddresses(size_t count) const;

    /**
     * @brief Get all known addresses
     */
    std::vector<NetAddress> getAllAddresses() const;

    /**
     * @brief Mark an address as good (successful connection)
     */
    void markGood(const NetAddress& address);

    /**
     * @brief Mark an address as bad (connection failed)
     */
    void markBad(const NetAddress& address);

    /**
     * @brief Remove an address
     */
    void remove(const NetAddress& address);

    /**
     * @brief Get number of known addresses
     */
    size_t size() const;

    /**
     * @brief Save to file
     */
    bool save(const std::string& filename) const;

    /**
     * @brief Load from file
     */
    bool load(const std::string& filename);

private:
    struct PeerInfo {
        NetAddress address;
        uint32_t successCount;
        uint32_t failureCount;
        std::chrono::system_clock::time_point lastAttempt;
    };

    std::vector<PeerInfo> peers_;
};

}  // namespace network
}  // namespace ubuntu

// Hash function for NetAddress (for unordered containers)
namespace std {
template <>
struct hash<ubuntu::network::NetAddress> {
    size_t operator()(const ubuntu::network::NetAddress& addr) const noexcept {
        return std::hash<std::string>{}(addr.ip) ^ std::hash<uint16_t>{}(addr.port);
    }
};
}  // namespace std
