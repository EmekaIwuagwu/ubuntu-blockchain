#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace rpc {

// Forward declarations
class JsonValue;
class AuthenticationManager;

/**
 * @brief JSON value type for RPC
 *
 * Simple JSON representation for RPC methods.
 */
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue();
    explicit JsonValue(bool value);
    explicit JsonValue(int64_t value);
    explicit JsonValue(double value);
    explicit JsonValue(const std::string& value);
    explicit JsonValue(const char* value);
    ~JsonValue();

    // Type checking
    Type getType() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    // Value getters
    bool getBool() const;
    int64_t getInt() const;
    double getDouble() const;
    std::string getString() const;

    // Array operations
    void pushBack(const JsonValue& value);
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;
    size_t size() const;

    // Object operations
    void set(const std::string& key, const JsonValue& value);
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    bool has(const std::string& key) const;
    std::vector<std::string> keys() const;

    // Serialization
    std::string toJsonString() const;
    static JsonValue fromJsonString(const std::string& json);

    // Factory methods
    static JsonValue makeArray();
    static JsonValue makeObject();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief RPC method handler
 *
 * Function signature for RPC method implementations.
 */
using RpcMethod = std::function<JsonValue(const JsonValue& params)>;

/**
 * @brief JSON-RPC 2.0 server
 *
 * Implements JSON-RPC 2.0 specification for blockchain interaction.
 */
class RpcServer {
public:
    /**
     * @brief Construct RPC server
     *
     * @param bindAddress Address to bind to (e.g., "127.0.0.1")
     * @param port Port to listen on (default: 8332)
     */
    RpcServer(const std::string& bindAddress = "127.0.0.1", uint16_t port = 8332);
    ~RpcServer();

    /**
     * @brief Register an RPC method
     *
     * @param name Method name
     * @param method Method handler function
     */
    void registerMethod(const std::string& name, RpcMethod method);

    /**
     * @brief Start the RPC server
     *
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the RPC server
     */
    void stop();

    /**
     * @brief Check if server is running
     *
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Process a JSON-RPC request
     *
     * @param request JSON-RPC request string
     * @return JSON-RPC response string
     */
    std::string processRequest(const std::string& request);

    /**
     * @brief Set authentication credentials
     *
     * @param username RPC username
     * @param password RPC password
     */
    void setAuth(const std::string& username, const std::string& password);

    /**
     * @brief Enable/disable authentication
     *
     * @param enabled true to require authentication
     */
    void setAuthEnabled(bool enabled);

    /**
     * @brief Process authenticated request
     *
     * @param request JSON-RPC request string
     * @param authToken Session token for authentication (optional)
     * @param clientIP Client IP address for rate limiting (optional)
     * @return JSON-RPC response string
     */
    std::string processAuthenticatedRequest(const std::string& request,
                                           const std::string& authToken = "",
                                           const std::string& clientIP = "");

    /**
     * @brief Get authentication manager
     *
     * @return Reference to authentication manager
     */
    AuthenticationManager& getAuthManager();

    /**
     * @brief Get authentication manager (const)
     *
     * @return Const reference to authentication manager
     */
    const AuthenticationManager& getAuthManager() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Handle a single RPC call
     *
     * @param request Parsed JSON-RPC request
     * @return JSON-RPC response
     */
    JsonValue handleCall(const JsonValue& request);

    /**
     * @brief Create error response
     *
     * @param id Request ID
     * @param code Error code
     * @param message Error message
     * @return JSON-RPC error response
     */
    JsonValue createErrorResponse(const JsonValue& id, int code, const std::string& message);

    /**
     * @brief Create success response
     *
     * @param id Request ID
     * @param result Result value
     * @return JSON-RPC success response
     */
    JsonValue createSuccessResponse(const JsonValue& id, const JsonValue& result);
};

/**
 * @brief Standard JSON-RPC error codes
 */
namespace ErrorCode {
constexpr int PARSE_ERROR = -32700;
constexpr int INVALID_REQUEST = -32600;
constexpr int METHOD_NOT_FOUND = -32601;
constexpr int INVALID_PARAMS = -32602;
constexpr int INTERNAL_ERROR = -32603;

// Application-specific errors (Bitcoin-compatible)
constexpr int RPC_MISC_ERROR = -1;
constexpr int RPC_TYPE_ERROR = -3;
constexpr int RPC_INVALID_ADDRESS_OR_KEY = -5;
constexpr int RPC_OUT_OF_MEMORY = -7;
constexpr int RPC_INVALID_PARAMETER = -8;
constexpr int RPC_DATABASE_ERROR = -20;
constexpr int RPC_DESERIALIZATION_ERROR = -22;
constexpr int RPC_VERIFY_ERROR = -25;
constexpr int RPC_WALLET_ERROR = -4;
constexpr int RPC_WALLET_INSUFFICIENT_FUNDS = -6;
constexpr int RPC_WALLET_INVALID_LABEL_NAME = -11;
constexpr int RPC_WALLET_KEYPOOL_RAN_OUT = -12;
constexpr int RPC_WALLET_UNLOCK_NEEDED = -13;
constexpr int RPC_WALLET_PASSPHRASE_INCORRECT = -14;
constexpr int RPC_WALLET_WRONG_ENC_STATE = -15;
constexpr int RPC_WALLET_ENCRYPTION_FAILED = -16;
constexpr int RPC_WALLET_ALREADY_UNLOCKED = -17;

// Authentication errors
constexpr int RPC_AUTH_REQUIRED = -18;
constexpr int RPC_AUTH_INVALID_CREDENTIALS = -19;
constexpr int RPC_AUTH_SESSION_EXPIRED = -32;
constexpr int RPC_AUTH_RATE_LIMIT = -33;
constexpr int RPC_AUTH_IP_BANNED = -34;
constexpr int RPC_AUTH_MAX_SESSIONS = -35;
}  // namespace ErrorCode

}  // namespace rpc
}  // namespace ubuntu
