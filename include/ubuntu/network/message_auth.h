#pragma once

#include "ubuntu/crypto/hash.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace network {

/**
 * @brief P2P Message authentication using HMAC-SHA256
 *
 * Implements authenticated P2P messaging to prevent MITM attacks.
 * Each peer establishes a shared secret during handshake, then all
 * subsequent messages are authenticated with HMAC-SHA256.
 */
class MessageAuthenticator {
public:
    // HMAC-SHA256 produces 32-byte tags
    static constexpr size_t AUTH_TAG_SIZE = 32;
    using AuthTag = std::array<uint8_t, AUTH_TAG_SIZE>;

    /**
     * @brief Authenticated message structure
     */
    struct AuthenticatedMessage {
        std::vector<uint8_t> payload;    // Original message
        AuthTag authTag;                 // HMAC-SHA256(key, payload)
        uint64_t sequence;               // Message sequence number (anti-replay)
        uint64_t timestamp;              // Unix timestamp (freshness check)
    };

    /**
     * @brief Initialize with shared secret
     *
     * @param sharedSecret Shared secret for HMAC (established during handshake)
     */
    explicit MessageAuthenticator(const std::vector<uint8_t>& sharedSecret);
    ~MessageAuthenticator();

    /**
     * @brief Sign a message with HMAC-SHA256
     *
     * @param message Message payload to sign
     * @param sequence Message sequence number
     * @return Authenticated message with HMAC tag
     */
    AuthenticatedMessage signMessage(const std::vector<uint8_t>& message,
                                     uint64_t sequence);

    /**
     * @brief Verify message authentication tag
     *
     * @param authMsg Authenticated message to verify
     * @param expectedSequence Expected sequence number (for anti-replay)
     * @param maxAge Maximum message age in seconds (default: 300 = 5 minutes)
     * @return true if message is authentic, false if forged or replay
     */
    bool verifyMessage(const AuthenticatedMessage& authMsg,
                      uint64_t expectedSequence,
                      uint64_t maxAge = 300);

    /**
     * @brief Serialize authenticated message for transmission
     *
     * Format: [payload_size:4][sequence:8][timestamp:8][auth_tag:32][payload:N]
     *
     * @param authMsg Authenticated message
     * @return Serialized bytes ready for network transmission
     */
    static std::vector<uint8_t> serialize(const AuthenticatedMessage& authMsg);

    /**
     * @brief Deserialize authenticated message from network bytes
     *
     * @param data Serialized message bytes
     * @return Authenticated message if valid format, nullopt if malformed
     */
    static std::optional<AuthenticatedMessage> deserialize(
        const std::vector<uint8_t>& data);

    /**
     * @brief Generate shared secret using Diffie-Hellman key exchange
     *
     * This should be called during the initial handshake between peers.
     *
     * @param myPrivateKey Our ephemeral private key
     * @param peerPublicKey Peer's ephemeral public key
     * @return Shared secret for HMAC
     */
    static std::vector<uint8_t> deriveSharedSecret(
        const std::vector<uint8_t>& myPrivateKey,
        const std::vector<uint8_t>& peerPublicKey);

    /**
     * @brief Constant-time comparison to prevent timing attacks
     *
     * @param a First byte array
     * @param b Second byte array
     * @return true if arrays are equal
     */
    static bool constantTimeCompare(const AuthTag& a, const AuthTag& b);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Compute HMAC-SHA256
     *
     * @param key HMAC key
     * @param data Data to authenticate
     * @return 32-byte HMAC tag
     */
    AuthTag computeHMAC(const std::vector<uint8_t>& data) const;

    /**
     * @brief Get current Unix timestamp
     *
     * @return Seconds since epoch
     */
    static uint64_t getCurrentTimestamp();
};

/**
 * @brief Peer identity and key management
 *
 * Manages peer identities, public keys, and shared secrets for authenticated
 * communication.
 */
class PeerIdentityManager {
public:
    /**
     * @brief Peer identity information
     */
    struct PeerIdentity {
        std::string peerId;                    // Peer unique identifier
        std::vector<uint8_t> publicKey;        // Peer's long-term public key
        std::vector<uint8_t> sharedSecret;     // Derived shared secret
        uint64_t establishedAt;                // When identity was verified
        bool verified;                          // Whether identity is verified
    };

    PeerIdentityManager();
    ~PeerIdentityManager();

    /**
     * @brief Add peer identity
     *
     * @param peerId Peer identifier
     * @param publicKey Peer's public key
     * @param sharedSecret Shared secret for this peer
     * @return true if added successfully
     */
    bool addPeer(const std::string& peerId,
                const std::vector<uint8_t>& publicKey,
                const std::vector<uint8_t>& sharedSecret);

    /**
     * @brief Get peer identity
     *
     * @param peerId Peer identifier
     * @return Peer identity if found
     */
    std::optional<PeerIdentity> getPeer(const std::string& peerId) const;

    /**
     * @brief Remove peer identity
     *
     * @param peerId Peer identifier
     */
    void removePeer(const std::string& peerId);

    /**
     * @brief Mark peer as verified
     *
     * @param peerId Peer identifier
     * @return true if peer exists and was marked verified
     */
    bool verifyPeer(const std::string& peerId);

    /**
     * @brief Check if peer is verified
     *
     * @param peerId Peer identifier
     * @return true if peer is verified
     */
    bool isPeerVerified(const std::string& peerId) const;

    /**
     * @brief Get number of verified peers
     *
     * @return Count of verified peers
     */
    size_t getVerifiedPeerCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace network
}  // namespace ubuntu
