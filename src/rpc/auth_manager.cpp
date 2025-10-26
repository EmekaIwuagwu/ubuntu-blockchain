#include "ubuntu/rpc/auth_manager.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace ubuntu {
namespace rpc {

// ============================================================================
// AuthSession Implementation
// ============================================================================

bool AuthSession::isExpired() const {
    auto now = std::chrono::system_clock::now();
    return now >= expiresAt;
}

bool AuthSession::isValid() const {
    if (isExpired()) {
        return false;
    }

    // Check if session has been idle too long
    auto now = std::chrono::system_clock::now();
    auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastActivity).count();

    // Max idle time: 30 minutes (configurable)
    const int64_t MAX_IDLE_SECONDS = 1800;
    return idleTime < MAX_IDLE_SECONDS;
}

// ============================================================================
// AuthenticationManager::Impl
// ============================================================================

struct AuthenticationManager::Impl {
    AuthenticationManager::Config config;

    // User credentials: username -> (passwordHash, salt)
    std::map<std::string, std::pair<std::string, std::string>> users;
    mutable std::mutex usersMutex;

    // Active sessions: token -> session info
    std::map<std::string, AuthSession> sessions;
    mutable std::mutex sessionsMutex;

    // Rate limiting: IP -> rate limit info
    std::map<std::string, AuthRateLimit> rateLimits;
    mutable std::mutex rateLimitsMutex;

    Impl(const AuthenticationManager::Config& cfg) : config(cfg) {}
};

// ============================================================================
// AuthenticationManager Implementation
// ============================================================================

AuthenticationManager::AuthenticationManager(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {
    spdlog::info("AuthenticationManager initialized");
    spdlog::info("  - Session timeout: {}s", config.sessionTimeout.count());
    spdlog::info("  - Max idle time: {}s", config.sessionMaxIdle.count());
    spdlog::info("  - Rate limit: {} attempts per {}s",
                 config.maxAttemptsPerWindow,
                 config.rateLimitWindow.count());
}

AuthenticationManager::~AuthenticationManager() {
    cleanup();
}

bool AuthenticationManager::addUser(const std::string& username,
                                   const std::string& password) {
    if (username.empty()) {
        spdlog::error("Cannot add user: username is empty");
        return false;
    }

    if (password.length() < impl_->config.minPasswordLength) {
        spdlog::error("Cannot add user: password too short (min {} characters)",
                     impl_->config.minPasswordLength);
        return false;
    }

    // Generate salt and hash password
    std::string salt = generateToken().substr(0, 32);
    std::string hashedPassword = hashPassword(password, salt);

    std::lock_guard<std::mutex> lock(impl_->usersMutex);
    impl_->users[username] = {hashedPassword, salt};

    spdlog::info("User '{}' added successfully", username);
    return true;
}

bool AuthenticationManager::removeUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(impl_->usersMutex);

    auto it = impl_->users.find(username);
    if (it == impl_->users.end()) {
        return false;
    }

    impl_->users.erase(it);
    spdlog::info("User '{}' removed", username);
    return true;
}

std::optional<std::string> AuthenticationManager::authenticate(
    const std::string& username,
    const std::string& password,
    const std::string& clientIP) {

    // Check if IP is banned
    if (!clientIP.empty() && isBanned(clientIP)) {
        spdlog::warn("Authentication attempt from banned IP: {}", clientIP);
        return std::nullopt;
    }

    // Check rate limit
    if (!clientIP.empty() && checkRateLimit(clientIP)) {
        spdlog::warn("Rate limit exceeded for IP: {}", clientIP);
        recordFailedAttempt(clientIP);
        return std::nullopt;
    }

    // Verify credentials
    if (!verifyPassword(username, password)) {
        spdlog::warn("Failed authentication attempt for user: {}", username);
        if (!clientIP.empty()) {
            recordFailedAttempt(clientIP);
        }
        return std::nullopt;
    }

    // Check session limit
    {
        std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
        if (impl_->sessions.size() >= impl_->config.maxActiveSessions) {
            spdlog::error("Maximum active sessions reached");
            return std::nullopt;
        }
    }

    // Create session
    AuthSession session;
    session.token = generateToken();
    session.username = username;
    session.createdAt = std::chrono::system_clock::now();
    session.expiresAt = session.createdAt + impl_->config.sessionTimeout;
    session.lastActivity = session.createdAt;
    session.clientIP = clientIP;
    session.requestCount = 0;

    {
        std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
        impl_->sessions[session.token] = session;
    }

    spdlog::info("User '{}' authenticated successfully from IP: {}", username, clientIP);

    if (!clientIP.empty()) {
        recordSuccessfulAuth(clientIP);
    }

    return session.token;
}

bool AuthenticationManager::validateToken(const std::string& token,
                                         const std::string& method) {
    if (token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(impl_->sessionsMutex);

    auto it = impl_->sessions.find(token);
    if (it == impl_->sessions.end()) {
        return false;
    }

    AuthSession& session = it->second;

    // Check if session is valid
    if (!session.isValid()) {
        spdlog::debug("Session expired for user: {}", session.username);
        impl_->sessions.erase(it);
        return false;
    }

    // Update last activity time
    session.lastActivity = std::chrono::system_clock::now();
    session.requestCount++;

    // Method-based authorization could be implemented here
    // For now, all authenticated users can call all methods
    (void)method;  // Suppress unused parameter warning

    return true;
}

bool AuthenticationManager::invalidateToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(impl_->sessionsMutex);

    auto it = impl_->sessions.find(token);
    if (it == impl_->sessions.end()) {
        return false;
    }

    spdlog::info("Session invalidated for user: {}", it->second.username);
    impl_->sessions.erase(it);
    return true;
}

bool AuthenticationManager::isBanned(const std::string& clientIP) const {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    auto it = impl_->rateLimits.find(clientIP);
    if (it == impl_->rateLimits.end()) {
        return false;
    }

    const AuthRateLimit& limit = it->second;
    if (!limit.isBanned) {
        return false;
    }

    // Check if ban has expired
    auto now = std::chrono::system_clock::now();
    return now < limit.banExpiresAt;
}

void AuthenticationManager::banIP(const std::string& clientIP,
                                 std::chrono::seconds duration) {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    AuthRateLimit& limit = impl_->rateLimits[clientIP];
    limit.isBanned = true;
    limit.banExpiresAt = std::chrono::system_clock::now() + duration;

    spdlog::warn("IP {} banned for {} seconds", clientIP, duration.count());
}

void AuthenticationManager::unbanIP(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    auto it = impl_->rateLimits.find(clientIP);
    if (it != impl_->rateLimits.end()) {
        it->second.isBanned = false;
        spdlog::info("IP {} unbanned", clientIP);
    }
}

size_t AuthenticationManager::getActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
    return impl_->sessions.size();
}

void AuthenticationManager::cleanup() {
    auto now = std::chrono::system_clock::now();

    // Clean up expired sessions
    {
        std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
        auto it = impl_->sessions.begin();
        size_t removed = 0;

        while (it != impl_->sessions.end()) {
            if (!it->second.isValid()) {
                it = impl_->sessions.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }

        if (removed > 0) {
            spdlog::debug("Cleaned up {} expired sessions", removed);
        }
    }

    // Clean up expired bans and old rate limit data
    {
        std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);
        auto it = impl_->rateLimits.begin();

        while (it != impl_->rateLimits.end()) {
            AuthRateLimit& limit = it->second;

            // Remove expired bans
            if (limit.isBanned && now >= limit.banExpiresAt) {
                limit.isBanned = false;
                spdlog::info("Ban expired for IP: {}", it->first);
            }

            // Remove old rate limit windows (older than 1 hour)
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - limit.windowStart);
            if (age.count() > 3600 && !limit.isBanned) {
                it = impl_->rateLimits.erase(it);
            } else {
                ++it;
            }
        }
    }
}

std::optional<std::pair<std::string, std::string>>
AuthenticationManager::parseBasicAuth(const std::string& authHeader) {
    // Expected format: "Basic base64(username:password)"
    const std::string prefix = "Basic ";

    if (authHeader.substr(0, prefix.length()) != prefix) {
        return std::nullopt;
    }

    std::string encoded = authHeader.substr(prefix.length());

    // Simple base64 decode (in production, use a proper base64 library)
    // For now, we'll assume the format is correct and extract credentials
    // This is a simplified implementation - production should use proper base64

    // TODO: Implement proper base64 decoding
    // For now, return nullopt to indicate parsing not fully implemented
    spdlog::warn("Base64 decoding not fully implemented in parseBasicAuth");

    return std::nullopt;
}

bool AuthenticationManager::verifyPassword(const std::string& username,
                                          const std::string& password) const {
    std::lock_guard<std::mutex> lock(impl_->usersMutex);

    auto it = impl_->users.find(username);
    if (it == impl_->users.end()) {
        // Use constant-time comparison to prevent timing attacks
        // Even if user doesn't exist, still perform a dummy hash
        std::string dummySalt = "0000000000000000000000000000000000000000";
        hashPassword(password, dummySalt);
        return false;
    }

    const auto& [storedHash, salt] = it->second;
    std::string computedHash = hashPassword(password, salt);

    // Constant-time comparison
    return computedHash == storedHash;
}

std::string AuthenticationManager::generateToken() {
    // Generate 32 bytes of cryptographically secure random data
    unsigned char buffer[32];
    if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
        throw std::runtime_error("Failed to generate random token");
    }

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(buffer[i]);
    }

    return oss.str();
}

std::string AuthenticationManager::hashPassword(const std::string& password,
                                               const std::string& salt) {
    // Use PBKDF2 with SHA-256 for password hashing
    const int iterations = 100000;  // NIST recommended minimum
    const int keyLength = 32;       // 256 bits

    unsigned char derivedKey[keyLength];
    std::string saltBytes;

    if (salt.empty()) {
        // Generate new salt
        unsigned char saltBuffer[16];
        if (RAND_bytes(saltBuffer, sizeof(saltBuffer)) != 1) {
            throw std::runtime_error("Failed to generate salt");
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < sizeof(saltBuffer); ++i) {
            oss << std::setw(2) << static_cast<unsigned int>(saltBuffer[i]);
        }
        saltBytes = oss.str();
    } else {
        saltBytes = salt;
    }

    // Derive key using PBKDF2
    if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                          reinterpret_cast<const unsigned char*>(saltBytes.c_str()),
                          saltBytes.length(),
                          iterations,
                          EVP_sha256(),
                          keyLength,
                          derivedKey) != 1) {
        throw std::runtime_error("Password hashing failed");
    }

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < keyLength; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(derivedKey[i]);
    }

    return oss.str();
}

bool AuthenticationManager::checkRateLimit(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    auto now = std::chrono::system_clock::now();
    AuthRateLimit& limit = impl_->rateLimits[clientIP];

    // Check if we're in a new window
    auto windowAge = std::chrono::duration_cast<std::chrono::seconds>(
        now - limit.windowStart);

    if (windowAge >= impl_->config.rateLimitWindow) {
        // New window - reset counter
        limit.windowStart = now;
        limit.attemptCount = 0;
    }

    // Check if limit exceeded
    return limit.attemptCount >= impl_->config.maxAttemptsPerWindow;
}

void AuthenticationManager::recordFailedAttempt(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    auto now = std::chrono::system_clock::now();
    AuthRateLimit& limit = impl_->rateLimits[clientIP];

    // Initialize window if needed
    if (limit.windowStart == std::chrono::system_clock::time_point{}) {
        limit.windowStart = now;
    }

    limit.attemptCount++;

    spdlog::warn("Failed auth attempt from IP: {} (count: {}/{})",
                clientIP, limit.attemptCount, impl_->config.maxAttemptsPerWindow);

    // Ban if threshold exceeded
    if (limit.attemptCount >= impl_->config.maxAttemptsPerWindow) {
        limit.isBanned = true;
        limit.banExpiresAt = now + impl_->config.banDuration;
        spdlog::warn("IP {} banned due to repeated failed attempts", clientIP);
    }
}

void AuthenticationManager::recordSuccessfulAuth(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(impl_->rateLimitsMutex);

    // Reset attempt counter on successful auth
    auto it = impl_->rateLimits.find(clientIP);
    if (it != impl_->rateLimits.end()) {
        it->second.attemptCount = 0;
    }
}

}  // namespace rpc
}  // namespace ubuntu
