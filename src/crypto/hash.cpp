#include "ubuntu/crypto/hash.h"

#include <openssl/evp.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ubuntu {
namespace crypto {

// ============================================================================
// Hash256 Implementation
// ============================================================================

Hash256::Hash256() : data_{} {}

Hash256::Hash256(const DataType& data) : data_(data) {}

Hash256::Hash256(std::span<const uint8_t> data) {
    if (data.size() != SIZE) {
        throw std::invalid_argument("Hash256: invalid data size");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

Hash256 Hash256::zero() {
    return Hash256();
}

Hash256 Hash256::fromHex(const std::string& hex) {
    if (hex.length() != SIZE * 2) {
        throw std::invalid_argument("Hash256::fromHex: invalid hex string length");
    }

    DataType data;
    for (size_t i = 0; i < SIZE; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return Hash256(data);
}

std::string Hash256::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> Hash256::toVector() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

bool Hash256::operator==(const Hash256& other) const {
    return data_ == other.data_;
}

bool Hash256::operator!=(const Hash256& other) const {
    return data_ != other.data_;
}

bool Hash256::operator<(const Hash256& other) const {
    return data_ < other.data_;
}

bool Hash256::operator<=(const Hash256& other) const {
    return data_ <= other.data_;
}

bool Hash256::operator>(const Hash256& other) const {
    return data_ > other.data_;
}

bool Hash256::operator>=(const Hash256& other) const {
    return data_ >= other.data_;
}

// ============================================================================
// Hash160 Implementation
// ============================================================================

Hash160::Hash160() : data_{} {}

Hash160::Hash160(const DataType& data) : data_(data) {}

Hash160::Hash160(std::span<const uint8_t> data) {
    if (data.size() != SIZE) {
        throw std::invalid_argument("Hash160: invalid data size");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

Hash160 Hash160::zero() {
    return Hash160();
}

Hash160 Hash160::fromHex(const std::string& hex) {
    if (hex.length() != SIZE * 2) {
        throw std::invalid_argument("Hash160::fromHex: invalid hex string length");
    }

    DataType data;
    for (size_t i = 0; i < SIZE; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return Hash160(data);
}

std::string Hash160::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> Hash160::toVector() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

bool Hash160::operator==(const Hash160& other) const {
    return data_ == other.data_;
}

bool Hash160::operator!=(const Hash160& other) const {
    return data_ != other.data_;
}

bool Hash160::operator<(const Hash160& other) const {
    return data_ < other.data_;
}

// ============================================================================
// Hash Functions
// ============================================================================

Hash256 sha256(std::span<const uint8_t> data) {
    Hash256::DataType hash;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    SHA256_Final(hash.data(), &ctx);
    return Hash256(hash);
}

Hash256 sha256(const std::string& data) {
    return sha256(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

Hash256 sha256(const std::vector<uint8_t>& data) {
    return sha256(std::span<const uint8_t>(data.data(), data.size()));
}

Hash256 sha256d(std::span<const uint8_t> data) {
    // First SHA-256
    auto hash1 = sha256(data);

    // Second SHA-256
    return sha256(std::span<const uint8_t>(hash1.data().data(), Hash256::SIZE));
}

Hash256 sha256d(const std::string& data) {
    return sha256d(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

Hash256 sha256d(const std::vector<uint8_t>& data) {
    return sha256d(std::span<const uint8_t>(data.data(), data.size()));
}

Hash160 ripemd160(std::span<const uint8_t> data) {
    Hash160::DataType hash;
    RIPEMD160_CTX ctx;
    RIPEMD160_Init(&ctx);
    RIPEMD160_Update(&ctx, data.data(), data.size());
    RIPEMD160_Final(hash.data(), &ctx);
    return Hash160(hash);
}

Hash160 ripemd160(const std::string& data) {
    return ripemd160(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

Hash160 ripemd160(const std::vector<uint8_t>& data) {
    return ripemd160(std::span<const uint8_t>(data.data(), data.size()));
}

Hash160 hash160(std::span<const uint8_t> data) {
    // RIPEMD160(SHA256(data))
    auto sha = sha256(data);
    return ripemd160(std::span<const uint8_t>(sha.data().data(), Hash256::SIZE));
}

Hash160 hash160(const std::string& data) {
    return hash160(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    ));
}

Hash160 hash160(const std::vector<uint8_t>& data) {
    return hash160(std::span<const uint8_t>(data.data(), data.size()));
}

Hash256 hmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data) {
    Hash256::DataType hash;
    unsigned int hashLen = Hash256::SIZE;

    HMAC(EVP_sha256(),
         key.data(), key.size(),
         data.data(), data.size(),
         hash.data(), &hashLen);

    return Hash256(hash);
}

std::vector<uint8_t> pbkdf2Sha512(std::span<const uint8_t> password,
                                   std::span<const uint8_t> salt,
                                   int iterations,
                                   size_t keylen) {
    std::vector<uint8_t> derived(keylen);

    if (PKCS5_PBKDF2_HMAC(
            reinterpret_cast<const char*>(password.data()), password.size(),
            salt.data(), salt.size(),
            iterations,
            EVP_sha512(),
            keylen,
            derived.data()) != 1) {
        throw std::runtime_error("PBKDF2-HMAC-SHA512 failed");
    }

    return derived;
}

}  // namespace crypto
}  // namespace ubuntu
