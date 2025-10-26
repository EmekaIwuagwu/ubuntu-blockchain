#include "ubuntu/wallet/wallet_encryption.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <stdexcept>

namespace ubuntu {
namespace wallet {

// ============================================================================
// WalletEncryption Implementation
// ============================================================================

EncryptedData WalletEncryption::encrypt(const std::vector<uint8_t>& plaintext,
                                        const std::string& password,
                                        const Config& config) {
    if (plaintext.empty()) {
        throw std::runtime_error("Cannot encrypt empty plaintext");
    }

    if (password.empty()) {
        throw std::runtime_error("Cannot encrypt with empty password");
    }

    // Verify password strength
    auto [isValid, errorMsg] = verifyPasswordStrength(password);
    if (!isValid) {
        spdlog::warn("Weak password used for wallet encryption: {}", errorMsg);
    }

    EncryptedData result;
    result.iterations = config.pbkdf2Iterations;
    result.algorithm = "AES-256-GCM";
    result.version = 1;

    // Generate random salt and IV
    result.salt = generateRandomBytes(config.saltLength);
    result.iv = generateRandomBytes(config.ivLength);

    // Derive encryption key from password
    SecureBuffer<uint8_t> key(deriveKey(password, result.salt,
                                       config.pbkdf2Iterations,
                                       config.keyLength));

    // Encrypt data
    result.authTag.resize(config.authTagLength);
    result.ciphertext = encryptAES256GCM(plaintext, key.data(), result.iv, result.authTag);

    // Secure key is automatically wiped when SecureBuffer goes out of scope

    spdlog::info("Wallet data encrypted: {} bytes plaintext -> {} bytes ciphertext",
                 plaintext.size(), result.ciphertext.size());

    return result;
}

std::optional<std::vector<uint8_t>> WalletEncryption::decrypt(
    const EncryptedData& encrypted,
    const std::string& password) {

    if (encrypted.ciphertext.empty()) {
        spdlog::error("Cannot decrypt empty ciphertext");
        return std::nullopt;
    }

    if (password.empty()) {
        spdlog::error("Cannot decrypt with empty password");
        return std::nullopt;
    }

    if (encrypted.algorithm != "AES-256-GCM") {
        spdlog::error("Unsupported encryption algorithm: {}", encrypted.algorithm);
        return std::nullopt;
    }

    // Derive decryption key from password
    SecureBuffer<uint8_t> key(deriveKey(password, encrypted.salt,
                                       encrypted.iterations,
                                       32));  // AES-256 key size

    // Decrypt and verify data
    auto plaintext = decryptAES256GCM(encrypted.ciphertext, key.data(),
                                     encrypted.iv, encrypted.authTag);

    // Secure key is automatically wiped when SecureBuffer goes out of scope

    if (!plaintext.has_value()) {
        spdlog::error("Wallet decryption failed - invalid password or corrupted data");
        return std::nullopt;
    }

    spdlog::info("Wallet data decrypted: {} bytes ciphertext -> {} bytes plaintext",
                 encrypted.ciphertext.size(), plaintext->size());

    return plaintext;
}

std::vector<uint8_t> WalletEncryption::deriveKey(const std::string& password,
                                                 const std::vector<uint8_t>& salt,
                                                 uint32_t iterations,
                                                 uint32_t keyLength) {
    if (password.empty()) {
        throw std::runtime_error("Password cannot be empty");
    }

    if (salt.empty()) {
        throw std::runtime_error("Salt cannot be empty");
    }

    if (iterations < 10000) {
        spdlog::warn("PBKDF2 iterations ({}) below recommended minimum (10000)", iterations);
    }

    std::vector<uint8_t> derivedKey(keyLength);

    // Use PBKDF2-HMAC-SHA256 for key derivation
    if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                          salt.data(), salt.size(),
                          iterations,
                          EVP_sha256(),
                          keyLength,
                          derivedKey.data()) != 1) {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }

    return derivedKey;
}

std::vector<uint8_t> WalletEncryption::generateRandomBytes(size_t length) {
    if (length == 0) {
        throw std::runtime_error("Cannot generate zero random bytes");
    }

    std::vector<uint8_t> buffer(length);

    if (RAND_bytes(buffer.data(), length) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }

    return buffer;
}

void WalletEncryption::secureWipe(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
        data.clear();
    }
}

void WalletEncryption::secureWipe(std::string& str) {
    if (!str.empty()) {
        OPENSSL_cleanse(&str[0], str.size());
        str.clear();
    }
}

std::pair<bool, std::string> WalletEncryption::verifyPasswordStrength(
    const std::string& password,
    size_t minLength) {

    if (password.length() < minLength) {
        return {false, "Password too short (minimum " + std::to_string(minLength) + " characters)"};
    }

    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (char c : password) {
        if (std::isupper(c)) hasUpper = true;
        else if (std::islower(c)) hasLower = true;
        else if (std::isdigit(c)) hasDigit = true;
        else hasSpecial = true;
    }

    // Check for at least 3 of 4 character classes
    int complexity = hasUpper + hasLower + hasDigit + hasSpecial;
    if (complexity < 3) {
        return {false, "Password must contain at least 3 of: uppercase, lowercase, digits, special characters"};
    }

    return {true, ""};
}

std::vector<uint8_t> WalletEncryption::serialize(const EncryptedData& encrypted) {
    std::vector<uint8_t> result;

    // Format: [version:4][iterations:4][iv_len:4][salt_len:4][tag_len:4][ct_len:4]
    //         [algorithm_len:4][iv][salt][tag][ciphertext][algorithm]

    auto writeUint32 = [&result](uint32_t value) {
        result.push_back((value >> 24) & 0xFF);
        result.push_back((value >> 16) & 0xFF);
        result.push_back((value >> 8) & 0xFF);
        result.push_back(value & 0xFF);
    };

    // Write header
    writeUint32(encrypted.version);
    writeUint32(encrypted.iterations);
    writeUint32(encrypted.iv.size());
    writeUint32(encrypted.salt.size());
    writeUint32(encrypted.authTag.size());
    writeUint32(encrypted.ciphertext.size());
    writeUint32(encrypted.algorithm.size());

    // Write data
    result.insert(result.end(), encrypted.iv.begin(), encrypted.iv.end());
    result.insert(result.end(), encrypted.salt.begin(), encrypted.salt.end());
    result.insert(result.end(), encrypted.authTag.begin(), encrypted.authTag.end());
    result.insert(result.end(), encrypted.ciphertext.begin(), encrypted.ciphertext.end());
    result.insert(result.end(), encrypted.algorithm.begin(), encrypted.algorithm.end());

    return result;
}

std::optional<EncryptedData> WalletEncryption::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 28) {  // Minimum header size (7 * 4 bytes)
        spdlog::error("Encrypted data too small");
        return std::nullopt;
    }

    EncryptedData result;
    size_t offset = 0;

    auto readUint32 = [&data, &offset]() -> uint32_t {
        uint32_t value = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        return value;
    };

    // Read header
    result.version = readUint32();
    result.iterations = readUint32();
    uint32_t ivLen = readUint32();
    uint32_t saltLen = readUint32();
    uint32_t tagLen = readUint32();
    uint32_t ctLen = readUint32();
    uint32_t algoLen = readUint32();

    // Validate header
    if (result.version != 1) {
        spdlog::error("Unsupported encryption version: {}", result.version);
        return std::nullopt;
    }

    // Check if data has enough bytes
    size_t expectedSize = offset + ivLen + saltLen + tagLen + ctLen + algoLen;
    if (data.size() < expectedSize) {
        spdlog::error("Encrypted data corrupted - size mismatch");
        return std::nullopt;
    }

    // Read data
    auto readBytes = [&data, &offset](size_t len) -> std::vector<uint8_t> {
        std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + len);
        offset += len;
        return bytes;
    };

    result.iv = readBytes(ivLen);
    result.salt = readBytes(saltLen);
    result.authTag = readBytes(tagLen);
    result.ciphertext = readBytes(ctLen);

    // Read algorithm string
    result.algorithm = std::string(data.begin() + offset, data.begin() + offset + algoLen);
    offset += algoLen;

    return result;
}

std::vector<uint8_t> WalletEncryption::encryptAES256GCM(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv,
    std::vector<uint8_t>& authTag) {

    if (key.size() != 32) {
        throw std::runtime_error("AES-256 requires 32-byte key");
    }

    if (iv.size() != 12) {
        throw std::runtime_error("GCM mode requires 12-byte IV");
    }

    if (authTag.size() != 16) {
        throw std::runtime_error("GCM authentication tag must be 16 bytes");
    }

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
    int len = 0;
    int ciphertext_len = 0;

    try {
        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            throw std::runtime_error("EVP_EncryptInit_ex failed");
        }

        // Set IV length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
            throw std::runtime_error("Failed to set IV length");
        }

        // Initialize key and IV
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to set key and IV");
        }

        // Encrypt plaintext
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                             plaintext.data(), plaintext.size()) != 1) {
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
        ciphertext_len = len;

        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        }
        ciphertext_len += len;

        // Get authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, authTag.size(),
                               authTag.data()) != 1) {
            throw std::runtime_error("Failed to get authentication tag");
        }

        // Clean up
        EVP_CIPHER_CTX_free(ctx);

        // Resize to actual ciphertext size
        ciphertext.resize(ciphertext_len);
        return ciphertext;

    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

std::optional<std::vector<uint8_t>> WalletEncryption::decryptAES256GCM(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& authTag) {

    if (key.size() != 32) {
        spdlog::error("AES-256 requires 32-byte key");
        return std::nullopt;
    }

    if (iv.size() != 12) {
        spdlog::error("GCM mode requires 12-byte IV");
        return std::nullopt;
    }

    if (authTag.size() != 16) {
        spdlog::error("GCM authentication tag must be 16 bytes");
        return std::nullopt;
    }

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        spdlog::error("Failed to create cipher context");
        return std::nullopt;
    }

    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    int plaintext_len = 0;

    try {
        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("EVP_DecryptInit_ex failed");
            return std::nullopt;
        }

        // Set IV length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("Failed to set IV length");
            return std::nullopt;
        }

        // Initialize key and IV
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("Failed to set key and IV");
            return std::nullopt;
        }

        // Decrypt ciphertext
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                             ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("EVP_DecryptUpdate failed");
            return std::nullopt;
        }
        plaintext_len = len;

        // Set expected authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, authTag.size(),
                               const_cast<uint8_t*>(authTag.data())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("Failed to set authentication tag");
            return std::nullopt;
        }

        // Finalize decryption and verify authentication
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) <= 0) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("Authentication failed - invalid password or corrupted data");
            return std::nullopt;
        }
        plaintext_len += len;

        // Clean up
        EVP_CIPHER_CTX_free(ctx);

        // Resize to actual plaintext size
        plaintext.resize(plaintext_len);
        return plaintext;

    } catch (const std::exception& e) {
        EVP_CIPHER_CTX_free(ctx);
        spdlog::error("Decryption exception: {}", e.what());
        return std::nullopt;
    }
}

}  // namespace wallet
}  // namespace ubuntu
