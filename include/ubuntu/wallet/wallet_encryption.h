#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace wallet {

/**
 * @brief Encrypted data container
 */
struct EncryptedData {
    std::vector<uint8_t> ciphertext;     // Encrypted data
    std::vector<uint8_t> iv;             // Initialization vector (12 bytes for GCM)
    std::vector<uint8_t> salt;           // Salt for key derivation (32 bytes)
    std::vector<uint8_t> authTag;        // Authentication tag (16 bytes for GCM)
    uint32_t iterations;                  // PBKDF2 iterations
    std::string algorithm;                // Encryption algorithm (e.g., "AES-256-GCM")
    uint32_t version;                     // Encryption version for future compatibility

    EncryptedData()
        : iterations(0), algorithm("AES-256-GCM"), version(1) {}
};

/**
 * @brief Wallet encryption using AES-256-GCM
 *
 * Implements military-grade wallet encryption with:
 * - AES-256-GCM authenticated encryption
 * - PBKDF2-HMAC-SHA256 key derivation (100,000+ iterations)
 * - Cryptographically secure random IV and salt generation
 * - Authenticated encryption with integrity verification
 * - Secure memory wiping for sensitive data
 */
class WalletEncryption {
public:
    /**
     * @brief Encryption configuration
     */
    struct Config {
        uint32_t pbkdf2Iterations{100000};    // NIST recommended minimum
        uint32_t keyLength{32};               // 256 bits for AES-256
        uint32_t saltLength{32};              // 256 bits for salt
        uint32_t ivLength{12};                // 96 bits for GCM (recommended)
        uint32_t authTagLength{16};           // 128 bits for authentication tag
    };

    /**
     * @brief Encrypt data with password
     *
     * @param plaintext Data to encrypt
     * @param password User password
     * @param config Encryption configuration (optional)
     * @return Encrypted data container
     */
    static EncryptedData encrypt(const std::vector<uint8_t>& plaintext,
                                 const std::string& password,
                                 const Config& config = Config{});

    /**
     * @brief Decrypt data with password
     *
     * @param encrypted Encrypted data container
     * @param password User password
     * @return Decrypted data, or nullopt if decryption fails
     */
    static std::optional<std::vector<uint8_t>> decrypt(
        const EncryptedData& encrypted,
        const std::string& password);

    /**
     * @brief Derive encryption key from password
     *
     * @param password User password
     * @param salt Salt for key derivation
     * @param iterations PBKDF2 iterations
     * @param keyLength Desired key length in bytes
     * @return Derived key
     */
    static std::vector<uint8_t> deriveKey(const std::string& password,
                                          const std::vector<uint8_t>& salt,
                                          uint32_t iterations,
                                          uint32_t keyLength);

    /**
     * @brief Generate cryptographically secure random bytes
     *
     * @param length Number of bytes to generate
     * @return Random bytes
     */
    static std::vector<uint8_t> generateRandomBytes(size_t length);

    /**
     * @brief Securely wipe memory
     *
     * @param data Data to wipe
     */
    static void secureWipe(std::vector<uint8_t>& data);

    /**
     * @brief Securely wipe string
     *
     * @param str String to wipe
     */
    static void secureWipe(std::string& str);

    /**
     * @brief Verify password strength
     *
     * @param password Password to verify
     * @param minLength Minimum password length
     * @return Pair of (isValid, errorMessage)
     */
    static std::pair<bool, std::string> verifyPasswordStrength(
        const std::string& password,
        size_t minLength = 12);

    /**
     * @brief Serialize encrypted data to binary format
     *
     * @param encrypted Encrypted data
     * @return Serialized binary data
     */
    static std::vector<uint8_t> serialize(const EncryptedData& encrypted);

    /**
     * @brief Deserialize encrypted data from binary format
     *
     * @param data Serialized binary data
     * @return Encrypted data container, or nullopt if parsing fails
     */
    static std::optional<EncryptedData> deserialize(const std::vector<uint8_t>& data);

private:
    /**
     * @brief Encrypt with AES-256-GCM
     *
     * @param plaintext Data to encrypt
     * @param key Encryption key (32 bytes)
     * @param iv Initialization vector (12 bytes)
     * @param authTag Output authentication tag (16 bytes)
     * @return Ciphertext
     */
    static std::vector<uint8_t> encryptAES256GCM(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv,
        std::vector<uint8_t>& authTag);

    /**
     * @brief Decrypt with AES-256-GCM
     *
     * @param ciphertext Data to decrypt
     * @param key Encryption key (32 bytes)
     * @param iv Initialization vector (12 bytes)
     * @param authTag Authentication tag (16 bytes)
     * @return Plaintext, or nullopt if authentication fails
     */
    static std::optional<std::vector<uint8_t>> decryptAES256GCM(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& authTag);
};

/**
 * @brief RAII wrapper for secure memory
 *
 * Automatically wipes memory when object goes out of scope.
 */
template <typename T>
class SecureBuffer {
public:
    explicit SecureBuffer(size_t size) : data_(size) {}

    explicit SecureBuffer(const std::vector<T>& data) : data_(data) {}

    explicit SecureBuffer(std::vector<T>&& data) : data_(std::move(data)) {}

    ~SecureBuffer() {
        WalletEncryption::secureWipe(data_);
    }

    // Prevent copying
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Allow moving
    SecureBuffer(SecureBuffer&& other) noexcept : data_(std::move(other.data_)) {}
    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            WalletEncryption::secureWipe(data_);
            data_ = std::move(other.data_);
        }
        return *this;
    }

    std::vector<T>& data() { return data_; }
    const std::vector<T>& data() const { return data_; }

    T* ptr() { return data_.data(); }
    const T* ptr() const { return data_.data(); }

    size_t size() const { return data_.size(); }

private:
    std::vector<T> data_;
};

}  // namespace wallet
}  // namespace ubuntu
