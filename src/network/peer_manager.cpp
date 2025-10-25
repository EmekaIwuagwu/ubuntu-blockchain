#include "ubuntu/network/peer.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>

namespace ubuntu {
namespace network {

// ============================================================================
// NetAddress Implementation
// ============================================================================

std::string NetAddress::toString() const {
    std::ostringstream oss;
    oss << ip << ":" << port;
    return oss.str();
}

bool NetAddress::operator==(const NetAddress& other) const {
    return ip == other.ip && port == other.port;
}

bool NetAddress::operator<(const NetAddress& other) const {
    if (ip != other.ip) {
        return ip < other.ip;
    }
    return port < other.port;
}

// ============================================================================
// Peer Implementation
// ============================================================================

Peer::Peer(const NetAddress& address)
    : address_(address),
      state_(PeerState::DISCONNECTED),
      services_(0),
      protocolVersion_(0),
      startHeight_(0),
      nonce_(0),
      inbound_(false),
      bytesSent_(0),
      bytesReceived_(0),
      connectedTime_(std::chrono::system_clock::now()),
      lastMessageTime_(std::chrono::system_clock::now()),
      banScore_(0),
      latencyMs_(0) {}

Peer::~Peer() {
    disconnect();
}

bool Peer::connect() {
    if (isConnected()) {
        spdlog::warn("Already connected to {}", address_.toString());
        return true;
    }

    spdlog::info("Connecting to peer {}...", address_.toString());

    // TODO: Actual network connection logic would go here
    // For now, simulate successful connection
    state_ = PeerState::CONNECTED;
    connectedTime_ = std::chrono::system_clock::now();

    return true;
}

void Peer::disconnect() {
    if (state_ == PeerState::DISCONNECTED) {
        return;
    }

    spdlog::info("Disconnecting from peer {}", address_.toString());

    // TODO: Actual network disconnection logic
    state_ = PeerState::DISCONNECTED;
}

void Peer::increaseBanScore(uint32_t points) {
    banScore_ += points;

    if (banScore_ >= BAN_THRESHOLD) {
        spdlog::warn("Banning peer {} (score: {})", address_.toString(), banScore_);
        state_ = PeerState::BANNED;
        disconnect();
    }
}

void Peer::recordPing() {
    lastPingTime_ = std::chrono::system_clock::now();
}

void Peer::recordPong() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastPingTime_);
    latencyMs_ = duration.count();

    spdlog::debug("Peer {} latency: {}ms", address_.toString(), latencyMs_);
}

// ============================================================================
// PeerDatabase Implementation
// ============================================================================

PeerDatabase::PeerDatabase() = default;

PeerDatabase::~PeerDatabase() = default;

void PeerDatabase::addAddress(const NetAddress& address) {
    // Check if address already exists
    auto it = std::find_if(peers_.begin(), peers_.end(),
                            [&address](const PeerInfo& info) {
                                return info.address == address;
                            });

    if (it != peers_.end()) {
        // Update last seen time
        it->address.lastSeen = address.lastSeen;
        return;
    }

    // Add new address
    PeerInfo info;
    info.address = address;
    info.successCount = 0;
    info.failureCount = 0;
    info.lastAttempt = std::chrono::system_clock::time_point{};

    peers_.push_back(info);

    spdlog::debug("Added peer address: {}", address.toString());
}

std::vector<NetAddress> PeerDatabase::getRandomAddresses(size_t count) const {
    std::vector<NetAddress> result;

    if (peers_.empty()) {
        return result;
    }

    // Create a copy and shuffle
    std::vector<PeerInfo> shuffled = peers_;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(shuffled.begin(), shuffled.end(), g);

    // Take up to 'count' addresses
    size_t n = std::min(count, shuffled.size());
    for (size_t i = 0; i < n; ++i) {
        result.push_back(shuffled[i].address);
    }

    return result;
}

std::vector<NetAddress> PeerDatabase::getAllAddresses() const {
    std::vector<NetAddress> result;
    result.reserve(peers_.size());

    for (const auto& info : peers_) {
        result.push_back(info.address);
    }

    return result;
}

void PeerDatabase::markGood(const NetAddress& address) {
    auto it = std::find_if(peers_.begin(), peers_.end(),
                            [&address](const PeerInfo& info) {
                                return info.address == address;
                            });

    if (it != peers_.end()) {
        it->successCount++;
        it->lastAttempt = std::chrono::system_clock::now();
        spdlog::debug("Marked peer {} as good (successes: {})",
                      address.toString(), it->successCount);
    }
}

void PeerDatabase::markBad(const NetAddress& address) {
    auto it = std::find_if(peers_.begin(), peers_.end(),
                            [&address](const PeerInfo& info) {
                                return info.address == address;
                            });

    if (it != peers_.end()) {
        it->failureCount++;
        it->lastAttempt = std::chrono::system_clock::now();

        // Remove if too many failures
        if (it->failureCount > 10) {
            spdlog::info("Removing peer {} after {} failures",
                         address.toString(), it->failureCount);
            peers_.erase(it);
        }
    }
}

void PeerDatabase::remove(const NetAddress& address) {
    auto it = std::find_if(peers_.begin(), peers_.end(),
                            [&address](const PeerInfo& info) {
                                return info.address == address;
                            });

    if (it != peers_.end()) {
        peers_.erase(it);
        spdlog::debug("Removed peer address: {}", address.toString());
    }
}

size_t PeerDatabase::size() const {
    return peers_.size();
}

bool PeerDatabase::save(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open peer database file for writing: {}", filename);
        return false;
    }

    for (const auto& info : peers_) {
        file << info.address.ip << "," << info.address.port << ","
             << info.address.services << "," << info.successCount << ","
             << info.failureCount << "\n";
    }

    spdlog::info("Saved {} peer addresses to {}", peers_.size(), filename);
    return true;
}

bool PeerDatabase::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::warn("Peer database file not found: {}", filename);
        return false;
    }

    peers_.clear();
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string ip, portStr, servicesStr, successStr, failStr;

        if (std::getline(iss, ip, ',') &&
            std::getline(iss, portStr, ',') &&
            std::getline(iss, servicesStr, ',') &&
            std::getline(iss, successStr, ',') &&
            std::getline(iss, failStr, ',')) {

            PeerInfo info;
            info.address.ip = ip;
            info.address.port = static_cast<uint16_t>(std::stoi(portStr));
            info.address.services = std::stoull(servicesStr);
            info.successCount = std::stoul(successStr);
            info.failureCount = std::stoul(failStr);

            peers_.push_back(info);
        }
    }

    spdlog::info("Loaded {} peer addresses from {}", peers_.size(), filename);
    return true;
}

}  // namespace network
}  // namespace ubuntu
