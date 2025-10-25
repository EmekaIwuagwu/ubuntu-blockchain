#include "ubuntu/crypto/signatures.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ubuntu {
namespace crypto {

// ============================================================================
// Signature Implementation
// ============================================================================

Signature::Signature() {}

Signature::Signature(std::span<const uint8_t> derData)
    : data_(derData.begin(), derData.end()) {}

Signature::Signature(const std::vector<uint8_t>& derData) : data_(derData) {}

Signature Signature::fromDER(std::span<const uint8_t> der) {
    return Signature(der);
}

Signature Signature::fromCompact(std::span<const uint8_t> compact) {
    if (compact.size() != 64) {
        throw std::invalid_argument("Compact signature must be 64 bytes");
    }

    // Convert compact (r,s) to DER format
    // This is a simplified conversion
    ECDSA_SIG* sig = ECDSA_SIG_new();
    if (!sig) {
        throw std::runtime_error("Failed to create ECDSA_SIG");
    }

    BIGNUM* r = BN_bin2bn(compact.data(), 32, nullptr);
    BIGNUM* s = BN_bin2bn(compact.data() + 32, 32, nullptr);

    if (!r || !s) {
        ECDSA_SIG_free(sig);
        if (r) BN_free(r);
        if (s) BN_free(s);
        throw std::runtime_error("Failed to convert compact signature to BIGNUM");
    }

    ECDSA_SIG_set0(sig, r, s);

    // Convert to DER
    unsigned char* der = nullptr;
    int derLen = i2d_ECDSA_SIG(sig, &der);

    if (derLen <= 0) {
        ECDSA_SIG_free(sig);
        throw std::runtime_error("Failed to convert signature to DER");
    }

    std::vector<uint8_t> derData(der, der + derLen);
    OPENSSL_free(der);
    ECDSA_SIG_free(sig);

    return Signature(derData);
}

Signature Signature::fromHex(const std::string& hex) {
    size_t len = hex.length() / 2;
    std::vector<uint8_t> data(len);

    for (size_t i = 0; i < len; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return Signature(data);
}

bool Signature::isValid() const {
    if (data_.empty()) {
        return false;
    }

    // Parse DER signature to check validity
    const unsigned char* derPtr = data_.data();
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &derPtr, data_.size());

    if (!sig) {
        return false;
    }

    ECDSA_SIG_free(sig);
    return true;
}

std::vector<uint8_t> Signature::toDER() const {
    return data_;
}

std::vector<uint8_t> Signature::toCompact() const {
    // Convert DER to compact (r,s) format
    const unsigned char* derPtr = data_.data();
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &derPtr, data_.size());

    if (!sig) {
        throw std::runtime_error("Failed to parse DER signature");
    }

    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);

    std::vector<uint8_t> compact(64, 0);

    // Convert r and s to 32-byte arrays
    int rLen = BN_num_bytes(r);
    int sLen = BN_num_bytes(s);

    BN_bn2bin(r, compact.data() + (32 - rLen));
    BN_bn2bin(s, compact.data() + 32 + (32 - sLen));

    ECDSA_SIG_free(sig);

    return compact;
}

std::string Signature::toHex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

bool Signature::operator==(const Signature& other) const {
    return data_ == other.data_;
}

bool Signature::operator!=(const Signature& other) const {
    return data_ != other.data_;
}

// ============================================================================
// Signer Implementation
// ============================================================================

Signature Signer::sign(const Hash256& hash, const PrivateKey& privateKey) {
    if (!privateKey.isValid()) {
        throw std::invalid_argument("Invalid private key");
    }

    // Create EC_KEY
    EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ecKey) {
        throw std::runtime_error("Failed to create EC_KEY");
    }

    // Set private key
    BIGNUM* privBN = BN_bin2bn(privateKey.data().data(), PrivateKey::SIZE, nullptr);
    if (!privBN) {
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to convert private key to BIGNUM");
    }

    if (EC_KEY_set_private_key(ecKey, privBN) != 1) {
        BN_free(privBN);
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to set private key");
    }

    // Compute public key (required for EC_KEY)
    const EC_GROUP* group = EC_KEY_get0_group(ecKey);
    EC_POINT* pubKey = EC_POINT_new(group);
    if (!EC_POINT_mul(group, pubKey, privBN, nullptr, nullptr, nullptr)) {
        EC_POINT_free(pubKey);
        BN_free(privBN);
        EC_KEY_free(ecKey);
        throw std::runtime_error("Failed to compute public key");
    }

    EC_KEY_set_public_key(ecKey, pubKey);

    // Sign the hash
    ECDSA_SIG* sig = ECDSA_do_sign(hash.data().data(), Hash256::SIZE, ecKey);

    // Cleanup EC_KEY and BIGNUM
    EC_POINT_free(pubKey);
    BN_free(privBN);
    EC_KEY_free(ecKey);

    if (!sig) {
        throw std::runtime_error("ECDSA signing failed");
    }

    // Convert to DER format
    unsigned char* der = nullptr;
    int derLen = i2d_ECDSA_SIG(sig, &der);

    if (derLen <= 0) {
        ECDSA_SIG_free(sig);
        throw std::runtime_error("Failed to encode signature to DER");
    }

    std::vector<uint8_t> derData(der, der + derLen);
    OPENSSL_free(der);
    ECDSA_SIG_free(sig);

    return Signature(derData);
}

Signature Signer::signData(std::span<const uint8_t> data, const PrivateKey& privateKey) {
    auto hash = sha256d(data);
    return sign(hash, privateKey);
}

bool Signer::verify(const Hash256& hash,
                    const Signature& signature,
                    const PublicKey& publicKey) {
    if (!signature.isValid() || !publicKey.isValid()) {
        return false;
    }

    // Create EC_KEY
    EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ecKey) {
        return false;
    }

    // Set public key
    const EC_GROUP* group = EC_KEY_get0_group(ecKey);
    EC_POINT* pubPoint = EC_POINT_new(group);

    const unsigned char* pubData = publicKey.data().data();
    if (!EC_POINT_oct2point(group, pubPoint, pubData, publicKey.data().size(), nullptr)) {
        EC_POINT_free(pubPoint);
        EC_KEY_free(ecKey);
        return false;
    }

    if (EC_KEY_set_public_key(ecKey, pubPoint) != 1) {
        EC_POINT_free(pubPoint);
        EC_KEY_free(ecKey);
        return false;
    }

    // Parse signature
    const unsigned char* sigPtr = signature.data().data();
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &sigPtr, signature.data().size());

    if (!sig) {
        EC_POINT_free(pubPoint);
        EC_KEY_free(ecKey);
        return false;
    }

    // Verify signature
    int result = ECDSA_do_verify(hash.data().data(), Hash256::SIZE, sig, ecKey);

    // Cleanup
    ECDSA_SIG_free(sig);
    EC_POINT_free(pubPoint);
    EC_KEY_free(ecKey);

    return result == 1;
}

bool Signer::verifyData(std::span<const uint8_t> data,
                        const Signature& signature,
                        const PublicKey& publicKey) {
    auto hash = sha256d(data);
    return verify(hash, signature, publicKey);
}

bool Signer::batchVerify(const std::vector<Hash256>& hashes,
                         const std::vector<Signature>& signatures,
                         const std::vector<PublicKey>& publicKeys) {
    if (hashes.size() != signatures.size() || hashes.size() != publicKeys.size()) {
        return false;
    }

    // For now, verify each signature individually
    // A proper batch verification would use Schnorr or optimized ECDSA batch verification
    for (size_t i = 0; i < hashes.size(); ++i) {
        if (!verify(hashes[i], signatures[i], publicKeys[i])) {
            return false;
        }
    }

    return true;
}

std::optional<PublicKey> Signer::recoverPublicKey(const Hash256& hash,
                                                   const Signature& signature,
                                                   int recoveryId) {
    // Public key recovery is complex and requires full implementation
    // This is a placeholder that would need proper implementation
    // with recovery ID handling

    // For now, return empty optional
    return std::nullopt;
}

// ============================================================================
// Message Signing Implementation
// ============================================================================

namespace MessageSigning {

std::string signMessage(const std::string& message, const PrivateKey& privateKey) {
    // Bitcoin-style message signing
    std::string prefix = "\x18Ubuntu Blockchain Signed Message:\n";
    std::string fullMessage = prefix + std::to_string(message.length()) + message;

    auto hash = sha256d(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(fullMessage.data()),
        fullMessage.size()
    ));

    auto signature = Signer::sign(hash, privateKey);

    // Convert to base64 (placeholder - would need full base64 implementation)
    return signature.toHex();
}

bool verifyMessage(const std::string& message,
                   const std::string& signatureStr,
                   const PublicKey& publicKey) {
    // Bitcoin-style message verification
    std::string prefix = "\x18Ubuntu Blockchain Signed Message:\n";
    std::string fullMessage = prefix + std::to_string(message.length()) + message;

    auto hash = sha256d(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(fullMessage.data()),
        fullMessage.size()
    ));

    auto signature = Signature::fromHex(signatureStr);

    return Signer::verify(hash, signature, publicKey);
}

}  // namespace MessageSigning

}  // namespace crypto
}  // namespace ubuntu
