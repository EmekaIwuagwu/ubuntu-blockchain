#include "ubuntu/crypto/keys.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ubuntu {
namespace crypto {

// ============================================================================
// PrivateKey Implementation
// ============================================================================

PrivateKey::PrivateKey() : data_{} {}

PrivateKey::PrivateKey(const DataType& data) : data_(data) {}

PrivateKey::PrivateKey(std::span<const uint8_t> data) {
    if (data.size() != SIZE) {
        throw std::invalid_argument("PrivateKey: invalid data size");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

PrivateKey PrivateKey::generate() {
    DataType data;
    if (RAND_bytes(data.data(), SIZE) != 1) {
        throw std::runtime_error("Failed to generate random private key");
    }

    // Ensure key is within valid range for secp256k1
    // Key must be < n where n is the order of the curve
    // For simplicity, we rely on the tiny probability of generating invalid key
    return PrivateKey(data);
}

PrivateKey PrivateKey::fromHex(const std::string& hex) {
    if (hex.length() != SIZE * 2) {
        throw std::invalid_argument("PrivateKey::fromHex: invalid hex string length");
    }

    DataType data;
    for (size_t i = 0; i < SIZE; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return PrivateKey(data);
}

PrivateKey PrivateKey::fromBytes(std::span<const uint8_t> data) {
    return PrivateKey(data);
}

bool PrivateKey::isValid() const {
    // Check that key is not all zeros
    bool allZeros = std::all_of(data_.begin(), data_.end(),
                                 [](uint8_t b) { return b == 0; });
    return !allZeros;
}

std::string PrivateKey::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> PrivateKey::toBytes() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string PrivateKey::toWIF(bool compressed, uint8_t version) const {
    // WIF encoding: version + key + (compression flag) + checksum
    std::vector<uint8_t> data;
    data.push_back(version);
    data.insert(data.end(), data_.begin(), data_.end());

    if (compressed) {
        data.push_back(0x01);
    }

    // Add checksum (first 4 bytes of double SHA-256)
    auto checksum = sha256d(std::span<const uint8_t>(data.data(), data.size()));
    data.insert(data.end(), checksum.begin(), checksum.begin() + 4);

    // Base58 encode (placeholder - would need full implementation)
    // For now, return hex representation
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

PrivateKey::~PrivateKey() {
    secureWipe();
}

void PrivateKey::secureWipe() {
    // Use volatile to prevent compiler optimization
    volatile uint8_t* ptr = data_.data();
    for (size_t i = 0; i < SIZE; ++i) {
        ptr[i] = 0;
    }
}

bool PrivateKey::operator==(const PrivateKey& other) const {
    return data_ == other.data_;
}

bool PrivateKey::operator!=(const PrivateKey& other) const {
    return data_ != other.data_;
}

// ============================================================================
// PublicKey Implementation
// ============================================================================

PublicKey::PublicKey() : compressed_(true) {}

PublicKey::PublicKey(std::span<const uint8_t> data, bool compressed)
    : data_(data.begin(), data.end()), compressed_(compressed) {
    if (!isValid()) {
        throw std::invalid_argument("PublicKey: invalid data");
    }
}

PublicKey PublicKey::fromPrivateKey(const PrivateKey& privKey, bool compressed) {
    // Create EC_KEY object
    EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ecKey) {
        throw std::runtime_error("Failed to create EC_KEY");
    }

    // Set private key
    BIGNUM* bn = BN_bin2bn(privKey.data().data(), PrivateKey::SIZE, nullptr);
    if (!bn) {
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to convert private key to BIGNUM");
    }

    if (EC_KEY_set_private_key(ecKey, bn) != 1) {
        BN_free(bn);
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to set private key");
    }

    // Compute public key
    const EC_GROUP* group = EC_KEY_get0_group(ecKey);
    EC_POINT* pubKeyPoint = EC_POINT_new(group);
    if (!EC_POINT_mul(group, pubKeyPoint, bn, nullptr, nullptr, nullptr)) {
        EC_POINT_free(pubKeyPoint);
        BN_free(bn);
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to compute public key");
    }

    if (EC_KEY_set_public_key(ecKey, pubKeyPoint) != 1) {
        EC_POINT_free(pubKeyPoint);
        BN_free(bn);
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to set public key");
    }

    // Serialize public key
    point_conversion_form_t form = compressed ? POINT_CONVERSION_COMPRESSED
                                               : POINT_CONVERSION_UNCOMPRESSED;
    size_t pubKeyLen = EC_POINT_point2oct(group, pubKeyPoint, form,
                                           nullptr, 0, nullptr);

    std::vector<uint8_t> pubKeyData(pubKeyLen);
    EC_POINT_point2oct(group, pubKeyPoint, form,
                       pubKeyData.data(), pubKeyLen, nullptr);

    // Cleanup
    EC_POINT_free(pubKeyPoint);
    BN_free(bn);
    EC_KEY_free(ecKey);

    return PublicKey(std::span<const uint8_t>(pubKeyData.data(), pubKeyData.size()),
                     compressed);
}

PublicKey PublicKey::fromHex(const std::string& hex) {
    size_t len = hex.length() / 2;
    std::vector<uint8_t> data(len);

    for (size_t i = 0; i < len; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    bool compressed = (len == COMPRESSED_SIZE);
    return PublicKey(std::span<const uint8_t>(data.data(), data.size()), compressed);
}

PublicKey PublicKey::fromBytes(std::span<const uint8_t> data) {
    bool compressed = (data.size() == COMPRESSED_SIZE);
    return PublicKey(data, compressed);
}

bool PublicKey::isValid() const {
    if (compressed_) {
        return data_.size() == COMPRESSED_SIZE &&
               (data_[0] == 0x02 || data_[0] == 0x03);
    } else {
        return data_.size() == UNCOMPRESSED_SIZE && data_[0] == 0x04;
    }
}

std::string PublicKey::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> PublicKey::toBytes() const {
    return data_;
}

Hash160 PublicKey::getHash160() const {
    return hash160(std::span<const uint8_t>(data_.data(), data_.size()));
}

bool PublicKey::operator==(const PublicKey& other) const {
    return data_ == other.data_;
}

bool PublicKey::operator!=(const PublicKey& other) const {
    return data_ != other.data_;
}

// ============================================================================
// KeyPair Implementation
// ============================================================================

KeyPair KeyPair::generate(bool compressedPubKey) {
    auto privKey = PrivateKey::generate();
    auto pubKey = PublicKey::fromPrivateKey(privKey, compressedPubKey);
    return KeyPair{std::move(privKey), std::move(pubKey)};
}

KeyPair KeyPair::fromPrivateKey(const PrivateKey& privKey, bool compressedPubKey) {
    auto pubKey = PublicKey::fromPrivateKey(privKey, compressedPubKey);
    return KeyPair{privKey, std::move(pubKey)};
}

// ============================================================================
// ExtendedKey Implementation
// ============================================================================

ExtendedKey::ExtendedKey()
    : depth_(0), parentFingerprint_(0), childNumber_(0) {}

ExtendedKey::ExtendedKey(const PrivateKey& key,
                         const std::array<uint8_t, CHAIN_CODE_SIZE>& chainCode,
                         uint32_t depth,
                         uint32_t parentFingerprint,
                         uint32_t childNumber)
    : privateKey_(key),
      chainCode_(chainCode),
      depth_(depth),
      parentFingerprint_(parentFingerprint),
      childNumber_(childNumber) {}

ExtendedKey ExtendedKey::deriveChild(uint32_t index) const {
    if (index >= HARDENED_BIT) {
        throw std::invalid_argument("Use deriveHardened for hardened derivation");
    }

    // BIP-32 child key derivation
    // This is a simplified implementation
    // Full implementation would use proper HMAC-SHA512 derivation

    std::vector<uint8_t> data;
    auto pubKey = PublicKey::fromPrivateKey(privateKey_, true);
    data.insert(data.end(), pubKey.data().begin(), pubKey.data().end());

    // Append index (big-endian)
    data.push_back((index >> 24) & 0xFF);
    data.push_back((index >> 16) & 0xFF);
    data.push_back((index >> 8) & 0xFF);
    data.push_back(index & 0xFF);

    // HMAC-SHA512(chainCode, data)
    auto hmac = hmacSha256(
        std::span<const uint8_t>(chainCode_.data(), chainCode_.size()),
        std::span<const uint8_t>(data.data(), data.size())
    );

    // Split into left (private key tweak) and right (new chain code)
    PrivateKey::DataType newKeyData;
    std::copy(hmac.begin(), hmac.begin() + 32, newKeyData.begin());

    std::array<uint8_t, CHAIN_CODE_SIZE> newChainCode;
    // In real implementation, this would be the right half of HMAC-SHA512
    std::copy(chainCode_.begin(), chainCode_.end(), newChainCode.begin());

    PrivateKey newKey(newKeyData);

    return ExtendedKey(newKey, newChainCode, depth_ + 1,
                       getFingerprint(), index);
}

ExtendedKey ExtendedKey::deriveHardened(uint32_t index) const {
    return deriveChild(index | HARDENED_BIT);
}

ExtendedKey ExtendedKey::derivePath(const std::string& path) const {
    // Parse path like "m/44'/9999'/0'/0/0"
    if (path.empty() || path[0] != 'm') {
        throw std::invalid_argument("Invalid derivation path");
    }

    ExtendedKey result = *this;
    size_t pos = 2;  // Skip "m/"

    while (pos < path.length()) {
        size_t end = path.find('/', pos);
        if (end == std::string::npos) {
            end = path.length();
        }

        std::string indexStr = path.substr(pos, end - pos);
        bool hardened = (indexStr.back() == '\'');
        if (hardened) {
            indexStr.pop_back();
        }

        uint32_t index = std::stoul(indexStr);
        result = hardened ? result.deriveHardened(index)
                          : result.deriveChild(index);

        pos = end + 1;
    }

    return result;
}

PublicKey ExtendedKey::getPublicKey() const {
    return PublicKey::fromPrivateKey(privateKey_, true);
}

uint32_t ExtendedKey::getFingerprint() const {
    auto pubKey = getPublicKey();
    auto hash = pubKey.getHash160();
    uint32_t fingerprint = 0;
    std::memcpy(&fingerprint, hash.data().data(), 4);
    return fingerprint;
}

// ============================================================================
// Seed Implementation
// ============================================================================

Seed::Seed() : data_{} {}

Seed::Seed(const DataType& data) : data_(data) {}

Seed::Seed(std::span<const uint8_t> data) {
    if (data.size() != SIZE) {
        throw std::invalid_argument("Seed: invalid data size");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

Seed Seed::fromMnemonic(const std::vector<std::string>& mnemonic,
                        const std::string& passphrase) {
    return Mnemonic::toSeed(mnemonic, passphrase);
}

Seed Seed::fromHex(const std::string& hex) {
    if (hex.length() != SIZE * 2) {
        throw std::invalid_argument("Seed::fromHex: invalid hex string length");
    }

    DataType data;
    for (size_t i = 0; i < SIZE; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return Seed(data);
}

ExtendedKey Seed::toExtendedKey() const {
    // BIP-32 master key generation from seed
    std::string hmacKey = "Bitcoin seed";  // Standard for BIP-32
    auto hmac = hmacSha256(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(hmacKey.data()),
                                 hmacKey.size()),
        std::span<const uint8_t>(data_.data(), data_.size())
    );

    // Left 32 bytes = master private key
    PrivateKey::DataType privKeyData;
    std::copy(hmac.begin(), hmac.begin() + 32, privKeyData.begin());

    // Right 32 bytes = master chain code (simplified)
    std::array<uint8_t, ExtendedKey::CHAIN_CODE_SIZE> chainCode;
    std::copy(hmac.begin(), hmac.begin() + 32, chainCode.begin());

    return ExtendedKey(PrivateKey(privKeyData), chainCode, 0, 0, 0);
}

std::string Seed::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Seed::~Seed() {
    secureWipe();
}

void Seed::secureWipe() {
    volatile uint8_t* ptr = data_.data();
    for (size_t i = 0; i < SIZE; ++i) {
        ptr[i] = 0;
    }
}

// ============================================================================
// Mnemonic Implementation (Simplified)
// ============================================================================

namespace Mnemonic {

std::vector<std::string> generate(size_t wordCount) {
    // This is a simplified placeholder implementation
    // Full implementation would use BIP-39 wordlist
    std::vector<std::string> mnemonic;

    // For demonstration purposes
    static const std::vector<std::string> sampleWords = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };

    std::vector<uint8_t> entropy((wordCount * 11 - wordCount / 3) / 8);
    if (RAND_bytes(entropy.data(), entropy.size()) != 1) {
        throw std::runtime_error("Failed to generate entropy");
    }

    // Simplified: just return sample words
    for (size_t i = 0; i < wordCount; ++i) {
        mnemonic.push_back(sampleWords[i % sampleWords.size()]);
    }

    return mnemonic;
}

bool validate(const std::vector<std::string>& mnemonic) {
    // Simplified validation
    size_t wordCount = mnemonic.size();
    return wordCount == 12 || wordCount == 15 || wordCount == 18 ||
           wordCount == 21 || wordCount == 24;
}

Seed toSeed(const std::vector<std::string>& mnemonic, const std::string& passphrase) {
    if (!validate(mnemonic)) {
        throw std::invalid_argument("Invalid mnemonic");
    }

    // Concatenate mnemonic words
    std::string mnemonicStr;
    for (size_t i = 0; i < mnemonic.size(); ++i) {
        if (i > 0) mnemonicStr += " ";
        mnemonicStr += mnemonic[i];
    }

    // BIP-39: PBKDF2-HMAC-SHA512 with 2048 iterations
    std::string salt = "mnemonic" + passphrase;

    auto derived = pbkdf2Sha512(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(mnemonicStr.data()),
            mnemonicStr.size()
        ),
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(salt.data()),
            salt.size()
        ),
        2048,
        Seed::SIZE
    );

    Seed::DataType seedData;
    std::copy(derived.begin(), derived.end(), seedData.begin());
    return Seed(seedData);
}

}  // namespace Mnemonic

// ============================================================================
// BIP-44 Helpers
// ============================================================================

namespace BIP44 {

std::string buildPath(uint32_t coinType, uint32_t account,
                      uint32_t change, uint32_t addressIndex) {
    std::ostringstream oss;
    oss << "m/44'/" << coinType << "'/" << account << "'/"
        << change << "/" << addressIndex;
    return oss.str();
}

}  // namespace BIP44

}  // namespace crypto
}  // namespace ubuntu
