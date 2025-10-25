#pragma once

#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"

#include <span>
#include <vector>

namespace ubuntu {
namespace crypto {

/**
 * @brief ECDSA signature (DER-encoded or compact format)
 */
class Signature {
public:
    // DER-encoded signature (variable length, typically 70-72 bytes)
    Signature();
    explicit Signature(std::span<const uint8_t> derData);
    explicit Signature(const std::vector<uint8_t>& derData);

    // Factory methods
    static Signature fromDER(std::span<const uint8_t> der);
    static Signature fromCompact(std::span<const uint8_t> compact);  // 64 bytes: r + s
    static Signature fromHex(const std::string& hex);

    // Accessors
    const std::vector<uint8_t>& data() const { return data_; }
    bool isEmpty() const { return data_.empty(); }
    bool isValid() const;

    // Serialization
    std::vector<uint8_t> toDER() const;
    std::vector<uint8_t> toCompact() const;
    std::string toHex() const;

    // Comparison
    bool operator==(const Signature& other) const;
    bool operator!=(const Signature& other) const;

private:
    std::vector<uint8_t> data_;  // DER-encoded
};

/**
 * @brief ECDSA Signer - Sign and verify messages/transactions
 */
class Signer {
public:
    /**
     * @brief Sign a hash with a private key
     *
     * Uses RFC 6979 deterministic ECDSA signing for security.
     *
     * @param hash The hash to sign (typically SHA-256d of transaction)
     * @param privateKey The private key to sign with
     * @return ECDSA signature in DER format
     */
    static Signature sign(const Hash256& hash, const PrivateKey& privateKey);

    /**
     * @brief Sign raw data with a private key
     *
     * Computes SHA-256d of the data, then signs it.
     *
     * @param data The raw data to sign
     * @param privateKey The private key to sign with
     * @return ECDSA signature in DER format
     */
    static Signature signData(std::span<const uint8_t> data, const PrivateKey& privateKey);

    /**
     * @brief Verify a signature against a hash and public key
     *
     * @param hash The hash that was signed
     * @param signature The signature to verify
     * @param publicKey The public key to verify against
     * @return true if signature is valid, false otherwise
     */
    static bool verify(const Hash256& hash,
                       const Signature& signature,
                       const PublicKey& publicKey);

    /**
     * @brief Verify a signature against raw data and public key
     *
     * Computes SHA-256d of the data, then verifies signature.
     *
     * @param data The raw data that was signed
     * @param signature The signature to verify
     * @param publicKey The public key to verify against
     * @return true if signature is valid, false otherwise
     */
    static bool verifyData(std::span<const uint8_t> data,
                           const Signature& signature,
                           const PublicKey& publicKey);

    /**
     * @brief Batch verification of multiple signatures (optimization)
     *
     * More efficient than verifying signatures individually.
     * Uses optimized batch verification algorithm.
     *
     * @param hashes Vector of hashes
     * @param signatures Vector of signatures
     * @param publicKeys Vector of public keys
     * @return true if ALL signatures are valid, false if ANY is invalid
     */
    static bool batchVerify(const std::vector<Hash256>& hashes,
                            const std::vector<Signature>& signatures,
                            const std::vector<PublicKey>& publicKeys);

    /**
     * @brief Recover public key from signature (for compact signatures)
     *
     * Given a signature and the original hash, recover the public key
     * that created the signature. Used in some blockchain systems.
     *
     * @param hash The hash that was signed
     * @param signature The signature (must be compact format with recovery ID)
     * @param recoveryId Recovery ID (0-3)
     * @return Recovered public key, or empty if recovery fails
     */
    static std::optional<PublicKey> recoverPublicKey(const Hash256& hash,
                                                      const Signature& signature,
                                                      int recoveryId);
};

/**
 * @brief Message signing utilities (for signing arbitrary messages)
 */
namespace MessageSigning {

/**
 * @brief Sign a text message with a private key
 *
 * Creates a "Bitcoin signed message" format signature.
 *
 * @param message The text message to sign
 * @param privateKey The private key to sign with
 * @return Signature in base64 format
 */
std::string signMessage(const std::string& message, const PrivateKey& privateKey);

/**
 * @brief Verify a signed text message
 *
 * @param message The text message
 * @param signature The signature (base64 format)
 * @param publicKey The public key to verify against
 * @return true if signature is valid, false otherwise
 */
bool verifyMessage(const std::string& message,
                   const std::string& signature,
                   const PublicKey& publicKey);

}  // namespace MessageSigning

}  // namespace crypto
}  // namespace ubuntu
