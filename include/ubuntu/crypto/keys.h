#pragma once

#include "ubuntu/crypto/hash.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ubuntu {
namespace crypto {

/**
 * @brief 256-bit private key
 */
class PrivateKey {
public:
    static constexpr size_t SIZE = 32;
    using DataType = std::array<uint8_t, SIZE>;

    PrivateKey();
    explicit PrivateKey(const DataType& data);
    explicit PrivateKey(std::span<const uint8_t> data);

    // Factory methods
    static PrivateKey generate();
    static PrivateKey fromHex(const std::string& hex);
    static PrivateKey fromBytes(std::span<const uint8_t> data);

    // Accessors
    const DataType& data() const { return data_; }
    bool isValid() const;

    // Serialization
    std::string toHex() const;
    std::vector<uint8_t> toBytes() const;

    // WIF (Wallet Import Format) encoding
    std::string toWIF(bool compressed = true, uint8_t version = 0x80) const;
    static PrivateKey fromWIF(const std::string& wif);

    // Security: overwrite key data on destruction
    ~PrivateKey();

    // Comparison
    bool operator==(const PrivateKey& other) const;
    bool operator!=(const PrivateKey& other) const;

private:
    DataType data_;
    void secureWipe();
};

/**
 * @brief 33-byte compressed or 65-byte uncompressed public key
 */
class PublicKey {
public:
    static constexpr size_t COMPRESSED_SIZE = 33;
    static constexpr size_t UNCOMPRESSED_SIZE = 65;

    PublicKey();
    explicit PublicKey(std::span<const uint8_t> data, bool compressed = true);

    // Factory methods
    static PublicKey fromPrivateKey(const PrivateKey& privKey, bool compressed = true);
    static PublicKey fromHex(const std::string& hex);
    static PublicKey fromBytes(std::span<const uint8_t> data);

    // Accessors
    const std::vector<uint8_t>& data() const { return data_; }
    bool isCompressed() const { return compressed_; }
    bool isValid() const;

    // Serialization
    std::string toHex() const;
    std::vector<uint8_t> toBytes() const;

    // Compression
    PublicKey compress() const;
    PublicKey decompress() const;

    // Hashing for address generation
    Hash160 getHash160() const;

    // Comparison
    bool operator==(const PublicKey& other) const;
    bool operator!=(const PublicKey& other) const;

private:
    std::vector<uint8_t> data_;
    bool compressed_;
};

/**
 * @brief Key pair (private + public key)
 */
struct KeyPair {
    PrivateKey privateKey;
    PublicKey publicKey;

    // Generate a new random key pair
    static KeyPair generate(bool compressedPubKey = true);

    // Derive public key from existing private key
    static KeyPair fromPrivateKey(const PrivateKey& privKey, bool compressedPubKey = true);
};

/**
 * @brief BIP-32 extended key (for HD wallet derivation)
 */
class ExtendedKey {
public:
    static constexpr size_t CHAIN_CODE_SIZE = 32;

    ExtendedKey();
    ExtendedKey(const PrivateKey& key,
                const std::array<uint8_t, CHAIN_CODE_SIZE>& chainCode,
                uint32_t depth = 0,
                uint32_t parentFingerprint = 0,
                uint32_t childNumber = 0);

    // BIP-32 derivation
    ExtendedKey deriveChild(uint32_t index) const;
    ExtendedKey deriveHardened(uint32_t index) const;

    // Path derivation (e.g., "m/44'/9999'/0'/0/0")
    ExtendedKey derivePath(const std::string& path) const;

    // Accessors
    const PrivateKey& getPrivateKey() const { return privateKey_; }
    PublicKey getPublicKey() const;
    const std::array<uint8_t, CHAIN_CODE_SIZE>& getChainCode() const { return chainCode_; }
    uint32_t getDepth() const { return depth_; }
    uint32_t getFingerprint() const;

    // Serialization (BIP-32 format)
    std::string serialize(bool isPrivate = true, uint32_t version = 0x0488ADE4) const;
    static ExtendedKey deserialize(const std::string& serialized);

private:
    PrivateKey privateKey_;
    std::array<uint8_t, CHAIN_CODE_SIZE> chainCode_;
    uint32_t depth_;
    uint32_t parentFingerprint_;
    uint32_t childNumber_;

    static constexpr uint32_t HARDENED_BIT = 0x80000000;
};

/**
 * @brief Seed for deterministic key generation
 */
class Seed {
public:
    static constexpr size_t SIZE = 64;  // 512 bits for BIP-39
    using DataType = std::array<uint8_t, SIZE>;

    Seed();
    explicit Seed(const DataType& data);
    explicit Seed(std::span<const uint8_t> data);

    // Factory methods
    static Seed fromMnemonic(const std::vector<std::string>& mnemonic,
                             const std::string& passphrase = "");
    static Seed fromHex(const std::string& hex);

    // Accessors
    const DataType& data() const { return data_; }

    // Derive master extended key (BIP-32)
    ExtendedKey toExtendedKey() const;

    // Serialization
    std::string toHex() const;

    // Security: overwrite seed data on destruction
    ~Seed();

private:
    DataType data_;
    void secureWipe();
};

/**
 * @brief BIP-39 Mnemonic support
 */
namespace Mnemonic {

/**
 * @brief Generate a random mnemonic
 *
 * @param wordCount Number of words (12, 15, 18, 21, or 24)
 * @return Vector of mnemonic words
 */
std::vector<std::string> generate(size_t wordCount = 12);

/**
 * @brief Validate a mnemonic
 *
 * @param mnemonic Vector of mnemonic words
 * @return true if valid, false otherwise
 */
bool validate(const std::vector<std::string>& mnemonic);

/**
 * @brief Convert mnemonic to seed
 *
 * @param mnemonic Vector of mnemonic words
 * @param passphrase Optional passphrase (default: "")
 * @return 512-bit seed
 */
Seed toSeed(const std::vector<std::string>& mnemonic, const std::string& passphrase = "");

}  // namespace Mnemonic

/**
 * @brief BIP-44 derivation path helpers
 */
namespace BIP44 {

// Ubuntu Blockchain coin type (needs to be registered)
constexpr uint32_t UBUNTU_COIN_TYPE = 9999;

/**
 * @brief Build a BIP-44 derivation path
 *
 * @param coinType Coin type (9999 for Ubuntu Blockchain)
 * @param account Account index
 * @param change 0 for external (receiving), 1 for internal (change)
 * @param addressIndex Address index
 * @return Derivation path string (e.g., "m/44'/9999'/0'/0/0")
 */
std::string buildPath(uint32_t coinType = UBUNTU_COIN_TYPE,
                      uint32_t account = 0,
                      uint32_t change = 0,
                      uint32_t addressIndex = 0);

}  // namespace BIP44

}  // namespace crypto
}  // namespace ubuntu
