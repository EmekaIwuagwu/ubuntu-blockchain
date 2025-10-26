#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ubuntu {
namespace network {

/**
 * @brief Token bucket rate limiter
 *
 * Implements a token bucket algorithm for rate limiting network messages.
 * Each peer has separate token buckets for different message types to prevent
 * resource exhaustion attacks.
 */
class RateLimiter {
public:
    /**
     * @brief Rate limit configuration for a message type
     */
    struct Limits {
        size_t maxTokens;        // Maximum tokens in bucket
        size_t refillRate;       // Tokens added per second
        size_t burstSize;        // Maximum burst size
        size_t costPerMessage;   // Token cost per message

        Limits(size_t max = 100, size_t rate = 10, size_t burst = 20, size_t cost = 1)
            : maxTokens(max), refillRate(rate), burstSize(burst), costPerMessage(cost) {}
    };

    /**
     * @brief Token bucket for a single peer
     */
    struct TokenBucket {
        size_t tokens;
        std::chrono::steady_clock::time_point lastRefill;
        size_t totalMessages;
        size_t totalBlocked;

        TokenBucket(size_t initialTokens = 100)
            : tokens(initialTokens),
              lastRefill(std::chrono::steady_clock::now()),
              totalMessages(0),
              totalBlocked(0) {}
    };

    /**
     * @brief Construct rate limiter with default limits
     */
    RateLimiter();

    /**
     * @brief Construct rate limiter with custom limits
     *
     * @param defaultLimits Default limits for all message types
     */
    explicit RateLimiter(const Limits& defaultLimits);

    ~RateLimiter();

    /**
     * @brief Try to consume tokens for a message
     *
     * @param peerId Peer identifier (IP:port)
     * @param messageType Message type (e.g., "inv", "tx", "block")
     * @param cost Token cost (defaults to 1)
     * @return true if message is allowed (tokens consumed), false if rate limited
     */
    bool tryConsume(const std::string& peerId,
                   const std::string& messageType = "default",
                   size_t cost = 1);

    /**
     * @brief Check if peer would be rate limited without consuming tokens
     *
     * @param peerId Peer identifier
     * @param messageType Message type
     * @param cost Token cost
     * @return true if peer has enough tokens
     */
    bool checkLimit(const std::string& peerId,
                   const std::string& messageType = "default",
                   size_t cost = 1) const;

    /**
     * @brief Set custom limits for a specific message type
     *
     * @param messageType Message type
     * @param limits Rate limits
     */
    void setLimits(const std::string& messageType, const Limits& limits);

    /**
     * @brief Get limits for a message type
     *
     * @param messageType Message type
     * @return Rate limits
     */
    Limits getLimits(const std::string& messageType) const;

    /**
     * @brief Get bucket information for a peer
     *
     * @param peerId Peer identifier
     * @param messageType Message type
     * @return Token bucket (or nullptr if doesn't exist)
     */
    const TokenBucket* getBucket(const std::string& peerId,
                                 const std::string& messageType) const;

    /**
     * @brief Reset rate limits for a peer
     *
     * @param peerId Peer identifier
     */
    void resetPeer(const std::string& peerId);

    /**
     * @brief Remove peer from rate limiter
     *
     * @param peerId Peer identifier
     */
    void removePeer(const std::string& peerId);

    /**
     * @brief Clean up old buckets for disconnected peers
     *
     * @param maxAge Maximum age before cleanup (seconds)
     */
    void cleanup(std::chrono::seconds maxAge = std::chrono::seconds(3600));

    /**
     * @brief Get statistics for a peer
     *
     * @param peerId Peer identifier
     * @return Pair of (total messages, total blocked)
     */
    std::pair<size_t, size_t> getStats(const std::string& peerId) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Refill tokens for a bucket
     *
     * @param bucket Token bucket
     * @param limits Rate limits
     */
    void refillTokens(TokenBucket& bucket, const Limits& limits);

    /**
     * @brief Get or create bucket for peer and message type
     *
     * @param peerId Peer identifier
     * @param messageType Message type
     * @return Reference to token bucket
     */
    TokenBucket& getOrCreateBucket(const std::string& peerId,
                                   const std::string& messageType);
};

/**
 * @brief Peer diversity manager
 *
 * Prevents Sybil attacks by enforcing limits on connections from
 * the same subnet or ASN.
 */
class PeerDiversityManager {
public:
    /**
     * @brief Configuration for peer diversity
     */
    struct Config {
        size_t maxPeersPerIPv4Subnet24{4};    // Max peers from same /24 subnet
        size_t maxPeersPerIPv4Subnet16{8};    // Max peers from same /16 subnet
        size_t maxPeersPerIPv6Subnet48{4};    // Max peers from same /48 subnet
        size_t maxInboundConnections{100};    // Max total inbound connections
        size_t maxOutboundConnections{25};    // Max total outbound connections
    };

    /**
     * @brief Construct peer diversity manager
     *
     * @param config Configuration
     */
    explicit PeerDiversityManager(const Config& config = Config{});

    ~PeerDiversityManager();

    /**
     * @brief Check if peer can be accepted
     *
     * @param address Peer IP address
     * @param isInbound true if inbound connection
     * @return true if peer can be accepted
     */
    bool canAcceptPeer(const std::string& address, bool isInbound = true) const;

    /**
     * @brief Add peer to diversity tracking
     *
     * @param address Peer IP address
     * @param isInbound true if inbound connection
     * @return true if peer was added
     */
    bool addPeer(const std::string& address, bool isInbound = true);

    /**
     * @brief Remove peer from diversity tracking
     *
     * @param address Peer IP address
     */
    void removePeer(const std::string& address);

    /**
     * @brief Get peer count by subnet
     *
     * @param address Peer IP address
     * @return Pair of (peers in /24, peers in /16)
     */
    std::pair<size_t, size_t> getSubnetCounts(const std::string& address) const;

    /**
     * @brief Get total inbound peer count
     *
     * @return Inbound peer count
     */
    size_t getInboundCount() const;

    /**
     * @brief Get total outbound peer count
     *
     * @return Outbound peer count
     */
    size_t getOutboundCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Extract subnet prefix from IP address
     *
     * @param address IP address
     * @param bits Subnet mask bits
     * @return Subnet prefix
     */
    static std::string getSubnetPrefix(const std::string& address, size_t bits);
};

}  // namespace network
}  // namespace ubuntu
