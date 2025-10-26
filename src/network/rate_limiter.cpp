#include "ubuntu/network/rate_limiter.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>

namespace ubuntu {
namespace network {

// ============================================================================
// RateLimiter::Impl
// ============================================================================

struct RateLimiter::Impl {
    // Per-peer, per-message-type token buckets
    // Key: peerId -> messageType -> TokenBucket
    std::map<std::string, std::map<std::string, TokenBucket>> buckets;
    mutable std::mutex bucketsMutex;

    // Rate limits per message type
    std::map<std::string, Limits> limits;
    mutable std::mutex limitsMutex;

    Limits defaultLimits;

    explicit Impl(const Limits& defaults) : defaultLimits(defaults) {
        // Set default limits for common message types
        // INV messages: High rate for block/tx announcements
        limits["inv"] = Limits(500, 50, 100, 1);

        // GETDATA messages: Medium rate
        limits["getdata"] = Limits(200, 20, 40, 1);

        // TX messages: High cost due to validation overhead
        limits["tx"] = Limits(100, 10, 20, 2);

        // BLOCK messages: Very high cost due to processing overhead
        limits["block"] = Limits(50, 5, 10, 5);

        // HEADERS messages: Medium rate
        limits["headers"] = Limits(200, 20, 40, 1);

        // GETHEADERS messages: Medium rate
        limits["getheaders"] = Limits(100, 10, 20, 1);

        // PING/PONG: Low rate
        limits["ping"] = Limits(50, 5, 10, 1);
        limits["pong"] = Limits(50, 5, 10, 1);

        // VERSION/VERACK: Very low rate (only needed once)
        limits["version"] = Limits(5, 1, 2, 1);
        limits["verack"] = Limits(5, 1, 2, 1);
    }
};

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter()
    : impl_(std::make_unique<Impl>(Limits(100, 10, 20, 1))) {
    spdlog::debug("RateLimiter initialized with default limits");
}

RateLimiter::RateLimiter(const Limits& defaultLimits)
    : impl_(std::make_unique<Impl>(defaultLimits)) {
    spdlog::debug("RateLimiter initialized with custom limits");
}

RateLimiter::~RateLimiter() = default;

bool RateLimiter::tryConsume(const std::string& peerId,
                            const std::string& messageType,
                            size_t cost) {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    // Get limits for message type
    Limits limits;
    {
        std::lock_guard<std::mutex> limitsLock(impl_->limitsMutex);
        auto it = impl_->limits.find(messageType);
        if (it != impl_->limits.end()) {
            limits = it->second;
        } else {
            limits = impl_->defaultLimits;
        }
    }

    // Get or create bucket
    TokenBucket& bucket = getOrCreateBucket(peerId, messageType);

    // Refill tokens based on elapsed time
    refillTokens(bucket, limits);

    // Update statistics
    bucket.totalMessages++;

    // Check if we have enough tokens
    if (bucket.tokens >= cost) {
        bucket.tokens -= cost;
        return true;
    }

    // Rate limited
    bucket.totalBlocked++;

    spdlog::warn("Rate limit exceeded for peer {} message type {} ({} tokens needed, {} available)",
                 peerId, messageType, cost, bucket.tokens);

    return false;
}

bool RateLimiter::checkLimit(const std::string& peerId,
                             const std::string& messageType,
                             size_t cost) const {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    // Check if bucket exists
    auto peerIt = impl_->buckets.find(peerId);
    if (peerIt == impl_->buckets.end()) {
        return true;  // No bucket yet, allow
    }

    auto bucketIt = peerIt->second.find(messageType);
    if (bucketIt == peerIt->second.end()) {
        return true;  // No bucket for this message type yet, allow
    }

    return bucketIt->second.tokens >= cost;
}

void RateLimiter::setLimits(const std::string& messageType, const Limits& limits) {
    std::lock_guard<std::mutex> lock(impl_->limitsMutex);
    impl_->limits[messageType] = limits;
    spdlog::debug("Set rate limits for message type {}: {} tokens, {} refill/s",
                  messageType, limits.maxTokens, limits.refillRate);
}

RateLimiter::Limits RateLimiter::getLimits(const std::string& messageType) const {
    std::lock_guard<std::mutex> lock(impl_->limitsMutex);

    auto it = impl_->limits.find(messageType);
    if (it != impl_->limits.end()) {
        return it->second;
    }

    return impl_->defaultLimits;
}

const RateLimiter::TokenBucket* RateLimiter::getBucket(
    const std::string& peerId,
    const std::string& messageType) const {

    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    auto peerIt = impl_->buckets.find(peerId);
    if (peerIt == impl_->buckets.end()) {
        return nullptr;
    }

    auto bucketIt = peerIt->second.find(messageType);
    if (bucketIt == peerIt->second.end()) {
        return nullptr;
    }

    return &bucketIt->second;
}

void RateLimiter::resetPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    auto it = impl_->buckets.find(peerId);
    if (it != impl_->buckets.end()) {
        // Reset all buckets for this peer
        for (auto& [messageType, bucket] : it->second) {
            Limits limits;
            {
                std::lock_guard<std::mutex> limitsLock(impl_->limitsMutex);
                auto limitsIt = impl_->limits.find(messageType);
                if (limitsIt != impl_->limits.end()) {
                    limits = limitsIt->second;
                } else {
                    limits = impl_->defaultLimits;
                }
            }

            bucket.tokens = limits.maxTokens;
            bucket.lastRefill = std::chrono::steady_clock::now();
        }

        spdlog::debug("Reset rate limits for peer {}", peerId);
    }
}

void RateLimiter::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    auto it = impl_->buckets.find(peerId);
    if (it != impl_->buckets.end()) {
        impl_->buckets.erase(it);
        spdlog::debug("Removed rate limiter buckets for peer {}", peerId);
    }
}

void RateLimiter::cleanup(std::chrono::seconds maxAge) {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    auto now = std::chrono::steady_clock::now();
    size_t removed = 0;

    auto it = impl_->buckets.begin();
    while (it != impl_->buckets.end()) {
        // Check if any bucket for this peer has been inactive too long
        bool allInactive = true;
        for (const auto& [messageType, bucket] : it->second) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - bucket.lastRefill);
            if (age < maxAge) {
                allInactive = false;
                break;
            }
        }

        if (allInactive) {
            it = impl_->buckets.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        spdlog::debug("Cleaned up rate limiter buckets for {} peers", removed);
    }
}

std::pair<size_t, size_t> RateLimiter::getStats(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(impl_->bucketsMutex);

    size_t totalMessages = 0;
    size_t totalBlocked = 0;

    auto peerIt = impl_->buckets.find(peerId);
    if (peerIt != impl_->buckets.end()) {
        for (const auto& [messageType, bucket] : peerIt->second) {
            totalMessages += bucket.totalMessages;
            totalBlocked += bucket.totalBlocked;
        }
    }

    return {totalMessages, totalBlocked};
}

void RateLimiter::refillTokens(TokenBucket& bucket, const Limits& limits) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - bucket.lastRefill);

    // Calculate tokens to add based on elapsed time
    double seconds = elapsed.count() / 1000.0;
    size_t tokensToAdd = static_cast<size_t>(seconds * limits.refillRate);

    if (tokensToAdd > 0) {
        bucket.tokens = std::min(bucket.tokens + tokensToAdd, limits.maxTokens);
        bucket.lastRefill = now;
    }
}

RateLimiter::TokenBucket& RateLimiter::getOrCreateBucket(
    const std::string& peerId,
    const std::string& messageType) {

    // Get limits for initial token count
    Limits limits;
    {
        std::lock_guard<std::mutex> limitsLock(impl_->limitsMutex);
        auto it = impl_->limits.find(messageType);
        if (it != impl_->limits.end()) {
            limits = it->second;
        } else {
            limits = impl_->defaultLimits;
        }
    }

    // Get or create bucket
    auto& peerBuckets = impl_->buckets[peerId];
    auto it = peerBuckets.find(messageType);

    if (it == peerBuckets.end()) {
        // Create new bucket with full tokens
        peerBuckets[messageType] = TokenBucket(limits.maxTokens);
        spdlog::debug("Created rate limiter bucket for peer {} message type {}",
                     peerId, messageType);
        return peerBuckets[messageType];
    }

    return it->second;
}

// ============================================================================
// PeerDiversityManager::Impl
// ============================================================================

struct PeerDiversityManager::Impl {
    Config config;

    // Subnet tracking: subnet -> peer count
    std::map<std::string, size_t> ipv4Subnet24;
    std::map<std::string, size_t> ipv4Subnet16;
    std::map<std::string, size_t> ipv6Subnet48;

    // Peer tracking: address -> isInbound
    std::map<std::string, bool> peers;

    mutable std::mutex mutex;

    explicit Impl(const Config& cfg) : config(cfg) {}
};

// ============================================================================
// PeerDiversityManager Implementation
// ============================================================================

PeerDiversityManager::PeerDiversityManager(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {
    spdlog::info("PeerDiversityManager initialized");
    spdlog::info("  - Max peers per /24: {}", config.maxPeersPerIPv4Subnet24);
    spdlog::info("  - Max peers per /16: {}", config.maxPeersPerIPv4Subnet16);
    spdlog::info("  - Max inbound: {}", config.maxInboundConnections);
    spdlog::info("  - Max outbound: {}", config.maxOutboundConnections);
}

PeerDiversityManager::~PeerDiversityManager() = default;

bool PeerDiversityManager::canAcceptPeer(const std::string& address,
                                        bool isInbound) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // Check if already connected
    if (impl_->peers.find(address) != impl_->peers.end()) {
        return false;
    }

    // Check total connection limits
    size_t inboundCount = 0;
    size_t outboundCount = 0;
    for (const auto& [addr, inbound] : impl_->peers) {
        if (inbound) {
            ++inboundCount;
        } else {
            ++outboundCount;
        }
    }

    if (isInbound && inboundCount >= impl_->config.maxInboundConnections) {
        return false;
    }

    if (!isInbound && outboundCount >= impl_->config.maxOutboundConnections) {
        return false;
    }

    // Check subnet diversity (IPv4 only for now)
    std::string subnet24 = getSubnetPrefix(address, 24);
    std::string subnet16 = getSubnetPrefix(address, 16);

    auto it24 = impl_->ipv4Subnet24.find(subnet24);
    if (it24 != impl_->ipv4Subnet24.end() &&
        it24->second >= impl_->config.maxPeersPerIPv4Subnet24) {
        spdlog::debug("Rejecting peer {} - too many peers from subnet /24", address);
        return false;
    }

    auto it16 = impl_->ipv4Subnet16.find(subnet16);
    if (it16 != impl_->ipv4Subnet16.end() &&
        it16->second >= impl_->config.maxPeersPerIPv4Subnet16) {
        spdlog::debug("Rejecting peer {} - too many peers from subnet /16", address);
        return false;
    }

    return true;
}

bool PeerDiversityManager::addPeer(const std::string& address, bool isInbound) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // Check if already exists
    if (impl_->peers.find(address) != impl_->peers.end()) {
        return false;
    }

    // Add peer
    impl_->peers[address] = isInbound;

    // Update subnet counts
    std::string subnet24 = getSubnetPrefix(address, 24);
    std::string subnet16 = getSubnetPrefix(address, 16);

    impl_->ipv4Subnet24[subnet24]++;
    impl_->ipv4Subnet16[subnet16]++;

    spdlog::debug("Added peer {} ({}) - /24: {}, /16: {}",
                 address, isInbound ? "inbound" : "outbound",
                 impl_->ipv4Subnet24[subnet24],
                 impl_->ipv4Subnet16[subnet16]);

    return true;
}

void PeerDiversityManager::removePeer(const std::string& address) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    auto it = impl_->peers.find(address);
    if (it == impl_->peers.end()) {
        return;
    }

    // Remove peer
    impl_->peers.erase(it);

    // Update subnet counts
    std::string subnet24 = getSubnetPrefix(address, 24);
    std::string subnet16 = getSubnetPrefix(address, 16);

    auto it24 = impl_->ipv4Subnet24.find(subnet24);
    if (it24 != impl_->ipv4Subnet24.end() && it24->second > 0) {
        it24->second--;
        if (it24->second == 0) {
            impl_->ipv4Subnet24.erase(it24);
        }
    }

    auto it16 = impl_->ipv4Subnet16.find(subnet16);
    if (it16 != impl_->ipv4Subnet16.end() && it16->second > 0) {
        it16->second--;
        if (it16->second == 0) {
            impl_->ipv4Subnet16.erase(it16);
        }
    }

    spdlog::debug("Removed peer {}", address);
}

std::pair<size_t, size_t> PeerDiversityManager::getSubnetCounts(
    const std::string& address) const {

    std::lock_guard<std::mutex> lock(impl_->mutex);

    std::string subnet24 = getSubnetPrefix(address, 24);
    std::string subnet16 = getSubnetPrefix(address, 16);

    size_t count24 = 0;
    size_t count16 = 0;

    auto it24 = impl_->ipv4Subnet24.find(subnet24);
    if (it24 != impl_->ipv4Subnet24.end()) {
        count24 = it24->second;
    }

    auto it16 = impl_->ipv4Subnet16.find(subnet16);
    if (it16 != impl_->ipv4Subnet16.end()) {
        count16 = it16->second;
    }

    return {count24, count16};
}

size_t PeerDiversityManager::getInboundCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    size_t count = 0;
    for (const auto& [addr, isInbound] : impl_->peers) {
        if (isInbound) {
            ++count;
        }
    }

    return count;
}

size_t PeerDiversityManager::getOutboundCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    size_t count = 0;
    for (const auto& [addr, isInbound] : impl_->peers) {
        if (!isInbound) {
            ++count;
        }
    }

    return count;
}

std::string PeerDiversityManager::getSubnetPrefix(const std::string& address,
                                                  size_t bits) {
    // Simple IPv4 subnet extraction
    // Format: xxx.xxx.xxx.xxx
    // /24 = xxx.xxx.xxx
    // /16 = xxx.xxx

    size_t dotCount = 0;
    size_t targetDots = 0;

    if (bits == 24) {
        targetDots = 3;
    } else if (bits == 16) {
        targetDots = 2;
    } else if (bits == 8) {
        targetDots = 1;
    } else {
        return address;  // Return full address if bits not recognized
    }

    std::string prefix;
    for (char c : address) {
        if (c == '.') {
            ++dotCount;
            if (dotCount >= targetDots) {
                break;
            }
        }
        prefix += c;
    }

    return prefix;
}

}  // namespace network
}  // namespace ubuntu
