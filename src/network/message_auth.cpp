// Copyright (c) 2025 Ubuntu Blockchain
// Distributed under the MIT software license

#include "ubuntu/network/message_auth.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace ubuntu {
namespace network {

// ============================================================================
// MessageAuthenticator Implementation
// ============================================================================

MessageAuthenticator::MessageAuthenticator(const std::vector<uint8_t>& sharedSecret)
    : sharedSecret_(sharedSecret) {
    if (sharedSecret.empty()) {
        throw std::invalid_argument("Shared secret cannot be empty");
    }
    if (sharedSecret.size() < 32) {
        throw std::invalid_argument("Shared secret must be at least 32 bytes");
    }
}

MessageAuthenticator::~MessageAuthenticator() {
    // Securely wipe shared secret from memory
    if (!sharedSecret_.empty()) {
        OPENSSL_cleanse(sharedSecret_.data(), sharedSecret_.size());
    }
}

MessageAuthenticator::AuthenticatedMessage MessageAuthenticator::signMessage(
    const std::vector<uint8_t>& message,
    uint64_t sequence) {

    AuthenticatedMessage authMsg;
    authMsg.payload = message;
    authMsg.sequence = sequence;
    authMsg.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL);

    // Create data to sign: payload || sequence || timestamp
    std::vector<uint8_t> dataToSign;
    dataToSign.reserve(message.size() + 16);
    dataToSign.insert(dataToSign.end(), message.begin(), message.end());

    // Append sequence (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        dataToSign.push_back(static_cast<uint8_t>((sequence >> (i * 8)) & 0xFF));
    }

    // Append timestamp (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        dataToSign.push_back(static_cast<uint8_t>((authMsg.timestamp >> (i * 8)) & 0xFF));
    }

    // Compute HMAC-SHA256
    unsigned int hmacLen = 0;
    unsigned char hmacResult[EVP_MAX_MD_SIZE];

    HMAC(EVP_sha256(),
         sharedSecret_.data(),
         static_cast<int>(sharedSecret_.size()),
         dataToSign.data(),
         dataToSign.size(),
         hmacResult,
         &hmacLen);

    if (hmacLen != AUTH_TAG_SIZE) {
        throw std::runtime_error("HMAC-SHA256 produced unexpected output size");
    }

    // Copy HMAC to auth tag
    std::copy(hmacResult, hmacResult + AUTH_TAG_SIZE, authMsg.authTag.begin());

    // Securely wipe intermediate data
    OPENSSL_cleanse(hmacResult, sizeof(hmacResult));

    return authMsg;
}

bool MessageAuthenticator::verifyMessage(
    const AuthenticatedMessage& authMsg,
    uint64_t expectedSequence,
    uint64_t maxAge) {

    // 1. Check sequence number (anti-replay)
    if (authMsg.sequence != expectedSequence) {
        return false;
    }

    // 2. Check timestamp freshness
    uint64_t currentTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL);

    if (authMsg.timestamp > currentTime + 60) {  // Allow 60s clock skew
        return false;
    }

    if (maxAge > 0 && currentTime > authMsg.timestamp + maxAge) {
        return false;
    }

    // 3. Recompute HMAC
    std::vector<uint8_t> dataToSign;
    dataToSign.reserve(authMsg.payload.size() + 16);
    dataToSign.insert(dataToSign.end(), authMsg.payload.begin(), authMsg.payload.end());

    // Append sequence (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        dataToSign.push_back(static_cast<uint8_t>((authMsg.sequence >> (i * 8)) & 0xFF));
    }

    // Append timestamp (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        dataToSign.push_back(static_cast<uint8_t>((authMsg.timestamp >> (i * 8)) & 0xFF));
    }

    unsigned int hmacLen = 0;
    unsigned char hmacResult[EVP_MAX_MD_SIZE];

    HMAC(EVP_sha256(),
         sharedSecret_.data(),
         static_cast<int>(sharedSecret_.size()),
         dataToSign.data(),
         dataToSign.size(),
         hmacResult,
         &hmacLen);

    if (hmacLen != AUTH_TAG_SIZE) {
        OPENSSL_cleanse(hmacResult, sizeof(hmacResult));
        return false;
    }

    // 4. Constant-time comparison to prevent timing attacks
    int result = CRYPTO_memcmp(hmacResult, authMsg.authTag.data(), AUTH_TAG_SIZE);

    // Securely wipe intermediate data
    OPENSSL_cleanse(hmacResult, sizeof(hmacResult));

    return result == 0;
}

std::vector<uint8_t> MessageAuthenticator::serialize(const AuthenticatedMessage& authMsg) {
    std::vector<uint8_t> result;

    // Format: [payload_size:4][payload:N][auth_tag:32][sequence:8][timestamp:8]
    uint32_t payloadSize = static_cast<uint32_t>(authMsg.payload.size());

    result.reserve(4 + payloadSize + AUTH_TAG_SIZE + 16);

    // Payload size (4 bytes, big-endian)
    result.push_back(static_cast<uint8_t>((payloadSize >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((payloadSize >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((payloadSize >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(payloadSize & 0xFF));

    // Payload
    result.insert(result.end(), authMsg.payload.begin(), authMsg.payload.end());

    // Auth tag
    result.insert(result.end(), authMsg.authTag.begin(), authMsg.authTag.end());

    // Sequence (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back(static_cast<uint8_t>((authMsg.sequence >> (i * 8)) & 0xFF));
    }

    // Timestamp (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back(static_cast<uint8_t>((authMsg.timestamp >> (i * 8)) & 0xFF));
    }

    return result;
}

std::optional<MessageAuthenticator::AuthenticatedMessage>
MessageAuthenticator::deserialize(const std::vector<uint8_t>& data) {
    // Minimum size: 4 (payload_size) + 32 (auth_tag) + 8 (sequence) + 8 (timestamp) = 52 bytes
    if (data.size() < 52) {
        return std::nullopt;
    }

    size_t offset = 0;

    // Parse payload size (4 bytes, big-endian)
    uint32_t payloadSize = 0;
    payloadSize |= static_cast<uint32_t>(data[offset++]) << 24;
    payloadSize |= static_cast<uint32_t>(data[offset++]) << 16;
    payloadSize |= static_cast<uint32_t>(data[offset++]) << 8;
    payloadSize |= static_cast<uint32_t>(data[offset++]);

    // Validate total size
    if (data.size() != 4 + payloadSize + AUTH_TAG_SIZE + 16) {
        return std::nullopt;
    }

    AuthenticatedMessage authMsg;

    // Parse payload
    authMsg.payload.assign(data.begin() + offset, data.begin() + offset + payloadSize);
    offset += payloadSize;

    // Parse auth tag
    std::copy(data.begin() + offset, data.begin() + offset + AUTH_TAG_SIZE,
              authMsg.authTag.begin());
    offset += AUTH_TAG_SIZE;

    // Parse sequence (8 bytes, big-endian)
    authMsg.sequence = 0;
    for (int i = 0; i < 8; ++i) {
        authMsg.sequence = (authMsg.sequence << 8) | data[offset++];
    }

    // Parse timestamp (8 bytes, big-endian)
    authMsg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        authMsg.timestamp = (authMsg.timestamp << 8) | data[offset++];
    }

    return authMsg;
}

std::vector<uint8_t> MessageAuthenticator::deriveSharedSecret(
    const std::vector<uint8_t>& localPrivateKey,
    const std::vector<uint8_t>& remotePubKey) {

    if (localPrivateKey.size() != 32) {
        throw std::invalid_argument("Local private key must be 32 bytes");
    }
    if (remotePubKey.size() != 65 && remotePubKey.size() != 33) {
        throw std::invalid_argument("Remote public key must be 33 or 65 bytes");
    }

    // Create EVP_PKEY from private key
    EVP_PKEY* localKey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr, localPrivateKey.data(), localPrivateKey.size());

    if (!localKey) {
        throw std::runtime_error("Failed to create local private key");
    }

    // Create EVP_PKEY from public key
    EVP_PKEY* remoteKey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr, remotePubKey.data(),
        remotePubKey.size() == 65 ? 32 : remotePubKey.size());

    if (!remoteKey) {
        EVP_PKEY_free(localKey);
        throw std::runtime_error("Failed to create remote public key");
    }

    // Create derivation context
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(localKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        throw std::runtime_error("Failed to create derivation context");
    }

    // Derive shared secret
    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        throw std::runtime_error("Failed to initialize key derivation");
    }

    if (EVP_PKEY_derive_set_peer(ctx, remoteKey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        throw std::runtime_error("Failed to set peer key");
    }

    // Determine buffer length
    size_t secretLen = 0;
    if (EVP_PKEY_derive(ctx, nullptr, &secretLen) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        throw std::runtime_error("Failed to determine shared secret length");
    }

    // Derive the shared secret
    std::vector<uint8_t> sharedSecret(secretLen);
    if (EVP_PKEY_derive(ctx, sharedSecret.data(), &secretLen) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        throw std::runtime_error("Failed to derive shared secret");
    }

    sharedSecret.resize(secretLen);

    // Cleanup
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(localKey);
    EVP_PKEY_free(remoteKey);

    return sharedSecret;
}

// ============================================================================
// PeerIdentityManager Implementation
// ============================================================================

bool PeerIdentityManager::addPeer(const std::string& peerId,
                                   const std::vector<uint8_t>& publicKey,
                                   const std::vector<uint8_t>& sharedSecret) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (peerId.empty() || publicKey.empty() || sharedSecret.empty()) {
        return false;
    }

    if (publicKey.size() != 33 && publicKey.size() != 65) {
        return false;  // Invalid public key size
    }

    if (sharedSecret.size() < 32) {
        return false;  // Shared secret too short
    }

    PeerIdentity identity;
    identity.peerId = peerId;
    identity.publicKey = publicKey;
    identity.sharedSecret = sharedSecret;
    identity.isVerified = false;
    identity.createdAt = std::chrono::system_clock::now();
    identity.lastSeenAt = identity.createdAt;
    identity.nextExpectedSequence = 0;

    peers_[peerId] = std::move(identity);
    return true;
}

bool PeerIdentityManager::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return false;
    }

    // Securely wipe shared secret before removing
    OPENSSL_cleanse(it->second.sharedSecret.data(), it->second.sharedSecret.size());

    peers_.erase(it);
    return true;
}

std::optional<PeerIdentityManager::PeerIdentity>
PeerIdentityManager::getPeer(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool PeerIdentityManager::verifyPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return false;
    }

    it->second.isVerified = true;
    return true;
}

bool PeerIdentityManager::updateLastSeen(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return false;
    }

    it->second.lastSeenAt = std::chrono::system_clock::now();
    return true;
}

bool PeerIdentityManager::incrementSequence(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return false;
    }

    it->second.nextExpectedSequence++;
    return true;
}

uint64_t PeerIdentityManager::getNextSequence(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peerId);
    if (it == peers_.end()) {
        return 0;
    }

    return it->second.nextExpectedSequence;
}

std::vector<std::string> PeerIdentityManager::getVerifiedPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    for (const auto& [peerId, identity] : peers_) {
        if (identity.isVerified) {
            result.push_back(peerId);
        }
    }

    return result;
}

size_t PeerIdentityManager::removeStaleP eers(std::chrono::seconds maxAge) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    size_t removed = 0;

    for (auto it = peers_.begin(); it != peers_.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastSeenAt);

        if (age > maxAge) {
            // Securely wipe shared secret
            OPENSSL_cleanse(it->second.sharedSecret.data(),
                          it->second.sharedSecret.size());
            it = peers_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

size_t PeerIdentityManager::getPeerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.size();
}

void PeerIdentityManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Securely wipe all shared secrets before clearing
    for (auto& [peerId, identity] : peers_) {
        OPENSSL_cleanse(identity.sharedSecret.data(), identity.sharedSecret.size());
    }

    peers_.clear();
}

}  // namespace network
}  // namespace ubuntu
