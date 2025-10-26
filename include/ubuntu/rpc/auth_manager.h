#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace rpc {

/**
 * @brief Authentication session information
 */
struct AuthSession {
    std::string token;
    std::string username;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point expiresAt;
    std::chrono::system_clock::time_point lastActivity;
    std::string clientIP;
    uint32_t requestCount;

    bool isExpired() const;
    bool isValid() const;
};

/**
 * @brief Rate limiting information for authentication attempts
 */
struct AuthRateLimit {
    std::chrono::system_clock::time_point windowStart;
    uint32_t attemptCount;
    bool isBanned;
    std::chrono::system_clock::time_point banExpiresAt;
};

/**
 * @brief Manages RPC authentication and session tokens
 *
 * Implements secure authentication with:
 * - HTTP Basic Auth support
 * - Session token management
 * - Rate limiting for brute-force protection
 * - Automatic session expiration
 * - IP-based banning for repeated failures
 */
class AuthenticationManager {
public:
    /**
     * @brief Configuration for authentication
     */
    struct Config {
        // Session configuration
        std::chrono::seconds sessionTimeout{3600};      // 1 hour
        std::chrono::seconds sessionMaxIdle{1800};      // 30 minutes
        uint32_t maxActiveSessions{100};

        // Rate limiting
        uint32_t maxAttemptsPerWindow{5};
        std::chrono::seconds rateLimitWindow{300};      // 5 minutes
        std::chrono::seconds banDuration{3600};         // 1 hour

        // Security
        bool requireAuth{true};
        bool allowBasicAuth{true};
        bool allowSessionTokens{true};
        uint32_t minPasswordLength{12};
    };

    /**
     * @brief Construct authentication manager
     *
     * @param config Authentication configuration
     */
    explicit AuthenticationManager(const Config& config = Config{});
    ~AuthenticationManager();

    /**
     * @brief Add or update user credentials
     *
     * @param username Username
     * @param password Password (will be hashed)
     * @return true if successful
     */
    bool addUser(const std::string& username, const std::string& password);

    /**
     * @brief Remove user credentials
     *
     * @param username Username to remove
     * @return true if user existed and was removed
     */
    bool removeUser(const std::string& username);

    /**
     * @brief Authenticate with username and password
     *
     * @param username Username
     * @param password Password
     * @param clientIP Client IP address for rate limiting
     * @return Session token if authentication successful
     */
    std::optional<std::string> authenticate(const std::string& username,
                                           const std::string& password,
                                           const std::string& clientIP = "");

    /**
     * @brief Validate a session token
     *
     * @param token Session token
     * @param method RPC method being called (for permission checking)
     * @return true if token is valid and authorized for the method
     */
    bool validateToken(const std::string& token, const std::string& method = "");

    /**
     * @brief Invalidate a session token (logout)
     *
     * @param token Session token to invalidate
     * @return true if token existed and was invalidated
     */
    bool invalidateToken(const std::string& token);

    /**
     * @brief Check if IP address is banned
     *
     * @param clientIP IP address to check
     * @return true if IP is currently banned
     */
    bool isBanned(const std::string& clientIP) const;

    /**
     * @brief Manually ban an IP address
     *
     * @param clientIP IP address to ban
     * @param duration Ban duration
     */
    void banIP(const std::string& clientIP, std::chrono::seconds duration);

    /**
     * @brief Unban an IP address
     *
     * @param clientIP IP address to unban
     */
    void unbanIP(const std::string& clientIP);

    /**
     * @brief Get active session count
     *
     * @return Number of active sessions
     */
    size_t getActiveSessionCount() const;

    /**
     * @brief Clean up expired sessions and rate limit data
     *
     * Should be called periodically by a cleanup thread
     */
    void cleanup();

    /**
     * @brief Parse HTTP Basic Auth header
     *
     * @param authHeader HTTP Authorization header value
     * @return Pair of (username, password) if valid, nullopt otherwise
     */
    static std::optional<std::pair<std::string, std::string>> parseBasicAuth(
        const std::string& authHeader);

    /**
     * @brief Verify password against stored hash
     *
     * @param username Username
     * @param password Password to verify
     * @return true if password matches
     */
    bool verifyPassword(const std::string& username, const std::string& password) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Generate a secure random token
     *
     * @return Cryptographically secure random token
     */
    std::string generateToken();

    /**
     * @brief Hash password for storage
     *
     * @param password Plaintext password
     * @param salt Salt (generated if empty)
     * @return Hashed password with salt
     */
    std::string hashPassword(const std::string& password, const std::string& salt = "");

    /**
     * @brief Check rate limit for IP address
     *
     * @param clientIP IP address
     * @return true if rate limit exceeded
     */
    bool checkRateLimit(const std::string& clientIP);

    /**
     * @brief Record failed authentication attempt
     *
     * @param clientIP IP address
     */
    void recordFailedAttempt(const std::string& clientIP);

    /**
     * @brief Record successful authentication
     *
     * @param clientIP IP address
     */
    void recordSuccessfulAuth(const std::string& clientIP);
};

}  // namespace rpc
}  // namespace ubuntu
