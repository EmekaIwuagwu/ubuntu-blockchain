#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ubuntu {
namespace crypto {

/**
 * @brief Fixed-size 256-bit hash value
 */
class Hash256 {
public:
    static constexpr size_t SIZE = 32;
    using DataType = std::array<uint8_t, SIZE>;

    Hash256();
    explicit Hash256(const DataType& data);
    explicit Hash256(std::span<const uint8_t> data);

    // Factory methods
    static Hash256 zero();
    static Hash256 fromHex(const std::string& hex);

    // Accessors
    const DataType& data() const { return data_; }
    DataType& data() { return data_; }
    const uint8_t* begin() const { return data_.data(); }
    const uint8_t* end() const { return data_.data() + SIZE; }
    size_t size() const { return SIZE; }

    // Conversion
    std::string toHex() const;
    std::vector<uint8_t> toVector() const;

    // Comparison operators
    bool operator==(const Hash256& other) const;
    bool operator!=(const Hash256& other) const;
    bool operator<(const Hash256& other) const;
    bool operator<=(const Hash256& other) const;
    bool operator>(const Hash256& other) const;
    bool operator>=(const Hash256& other) const;

private:
    DataType data_;
};

/**
 * @brief Fixed-size 160-bit hash value
 */
class Hash160 {
public:
    static constexpr size_t SIZE = 20;
    using DataType = std::array<uint8_t, SIZE>;

    Hash160();
    explicit Hash160(const DataType& data);
    explicit Hash160(std::span<const uint8_t> data);

    // Factory methods
    static Hash160 zero();
    static Hash160 fromHex(const std::string& hex);

    // Accessors
    const DataType& data() const { return data_; }
    DataType& data() { return data_; }
    const uint8_t* begin() const { return data_.data(); }
    const uint8_t* end() const { return data_.data() + SIZE; }
    size_t size() const { return SIZE; }

    // Conversion
    std::string toHex() const;
    std::vector<uint8_t> toVector() const;

    // Comparison operators
    bool operator==(const Hash160& other) const;
    bool operator!=(const Hash160& other) const;
    bool operator<(const Hash160& other) const;

private:
    DataType data_;
};

/**
 * @brief SHA-256 hash function
 *
 * Computes a single SHA-256 hash of the input data.
 */
Hash256 sha256(std::span<const uint8_t> data);
Hash256 sha256(const std::string& data);
Hash256 sha256(const std::vector<uint8_t>& data);

/**
 * @brief Double SHA-256 hash function (Bitcoin-style)
 *
 * Computes SHA-256(SHA-256(data)) - the standard hash used in Bitcoin
 * for proof-of-work and transaction/block hashing.
 */
Hash256 sha256d(std::span<const uint8_t> data);
Hash256 sha256d(const std::string& data);
Hash256 sha256d(const std::vector<uint8_t>& data);

/**
 * @brief RIPEMD-160 hash function
 *
 * Computes a RIPEMD-160 hash of the input data.
 * Used in Bitcoin address generation: RIPEMD160(SHA256(pubkey))
 */
Hash160 ripemd160(std::span<const uint8_t> data);
Hash160 ripemd160(const std::string& data);
Hash160 ripemd160(const std::vector<uint8_t>& data);

/**
 * @brief Hash160 of SHA-256 (Bitcoin address hash)
 *
 * Computes RIPEMD160(SHA256(data)) - the standard hash for Bitcoin addresses.
 */
Hash160 hash160(std::span<const uint8_t> data);
Hash160 hash160(const std::string& data);
Hash160 hash160(const std::vector<uint8_t>& data);

/**
 * @brief HMAC-SHA-256
 *
 * Computes HMAC-SHA-256 for key derivation and authentication.
 */
Hash256 hmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data);

/**
 * @brief PBKDF2-HMAC-SHA-512
 *
 * Key derivation function used in BIP-39 for mnemonic-to-seed conversion.
 *
 * @param password The password (mnemonic)
 * @param salt The salt value
 * @param iterations Number of iterations (2048 for BIP-39)
 * @param keylen Desired output length in bytes (64 for BIP-39)
 * @return Derived key
 */
std::vector<uint8_t> pbkdf2Sha512(std::span<const uint8_t> password,
                                   std::span<const uint8_t> salt,
                                   int iterations,
                                   size_t keylen);

}  // namespace crypto
}  // namespace ubuntu

// Hash function for std::unordered_map/set support
namespace std {
template <>
struct hash<ubuntu::crypto::Hash256> {
    size_t operator()(const ubuntu::crypto::Hash256& h) const noexcept {
        // Use first 8 bytes as hash value
        size_t result = 0;
        std::memcpy(&result, h.data().data(), sizeof(result));
        return result;
    }
};

template <>
struct hash<ubuntu::crypto::Hash160> {
    size_t operator()(const ubuntu::crypto::Hash160& h) const noexcept {
        size_t result = 0;
        std::memcpy(&result, h.data().data(), sizeof(result));
        return result;
    }
};
}  // namespace std
