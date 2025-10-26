#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/rpc/auth_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace ubuntu {
namespace rpc {

// ============================================================================
// JsonValue Implementation
// ============================================================================

struct JsonValue::Impl {
    using ValueType =
        std::variant<std::monostate, bool, int64_t, double, std::string,
                     std::vector<JsonValue>, std::map<std::string, JsonValue>>;

    ValueType value;

    Impl() : value(std::monostate{}) {}
};

JsonValue::JsonValue() : impl_(std::make_unique<Impl>()) {}

JsonValue::JsonValue(bool value) : impl_(std::make_unique<Impl>()) {
    impl_->value = value;
}

JsonValue::JsonValue(int64_t value) : impl_(std::make_unique<Impl>()) {
    impl_->value = value;
}

JsonValue::JsonValue(double value) : impl_(std::make_unique<Impl>()) {
    impl_->value = value;
}

JsonValue::JsonValue(const std::string& value) : impl_(std::make_unique<Impl>()) {
    impl_->value = value;
}

JsonValue::JsonValue(const char* value) : impl_(std::make_unique<Impl>()) {
    impl_->value = std::string(value);
}

JsonValue::~JsonValue() = default;

JsonValue::Type JsonValue::getType() const {
    if (std::holds_alternative<std::monostate>(impl_->value)) return Type::Null;
    if (std::holds_alternative<bool>(impl_->value)) return Type::Bool;
    if (std::holds_alternative<int64_t>(impl_->value)) return Type::Number;
    if (std::holds_alternative<double>(impl_->value)) return Type::Number;
    if (std::holds_alternative<std::string>(impl_->value)) return Type::String;
    if (std::holds_alternative<std::vector<JsonValue>>(impl_->value)) return Type::Array;
    if (std::holds_alternative<std::map<std::string, JsonValue>>(impl_->value))
        return Type::Object;
    return Type::Null;
}

bool JsonValue::isNull() const {
    return getType() == Type::Null;
}
bool JsonValue::isBool() const {
    return getType() == Type::Bool;
}
bool JsonValue::isNumber() const {
    return getType() == Type::Number;
}
bool JsonValue::isString() const {
    return getType() == Type::String;
}
bool JsonValue::isArray() const {
    return getType() == Type::Array;
}
bool JsonValue::isObject() const {
    return getType() == Type::Object;
}

bool JsonValue::getBool() const {
    if (auto* val = std::get_if<bool>(&impl_->value)) {
        return *val;
    }
    throw std::runtime_error("JsonValue is not a boolean");
}

int64_t JsonValue::getInt() const {
    if (auto* val = std::get_if<int64_t>(&impl_->value)) {
        return *val;
    }
    if (auto* val = std::get_if<double>(&impl_->value)) {
        return static_cast<int64_t>(*val);
    }
    throw std::runtime_error("JsonValue is not a number");
}

double JsonValue::getDouble() const {
    if (auto* val = std::get_if<double>(&impl_->value)) {
        return *val;
    }
    if (auto* val = std::get_if<int64_t>(&impl_->value)) {
        return static_cast<double>(*val);
    }
    throw std::runtime_error("JsonValue is not a number");
}

std::string JsonValue::getString() const {
    if (auto* val = std::get_if<std::string>(&impl_->value)) {
        return *val;
    }
    throw std::runtime_error("JsonValue is not a string");
}

void JsonValue::pushBack(const JsonValue& value) {
    if (!isArray()) {
        impl_->value = std::vector<JsonValue>();
    }
    std::get<std::vector<JsonValue>>(impl_->value).push_back(value);
}

JsonValue& JsonValue::operator[](size_t index) {
    if (!isArray()) {
        throw std::runtime_error("JsonValue is not an array");
    }
    return std::get<std::vector<JsonValue>>(impl_->value)[index];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (!isArray()) {
        throw std::runtime_error("JsonValue is not an array");
    }
    return std::get<std::vector<JsonValue>>(impl_->value)[index];
}

size_t JsonValue::size() const {
    if (isArray()) {
        return std::get<std::vector<JsonValue>>(impl_->value).size();
    }
    if (isObject()) {
        return std::get<std::map<std::string, JsonValue>>(impl_->value).size();
    }
    return 0;
}

void JsonValue::set(const std::string& key, const JsonValue& value) {
    if (!isObject()) {
        impl_->value = std::map<std::string, JsonValue>();
    }
    std::get<std::map<std::string, JsonValue>>(impl_->value)[key] = value;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!isObject()) {
        impl_->value = std::map<std::string, JsonValue>();
    }
    return std::get<std::map<std::string, JsonValue>>(impl_->value)[key];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!isObject()) {
        throw std::runtime_error("JsonValue is not an object");
    }
    return std::get<std::map<std::string, JsonValue>>(impl_->value).at(key);
}

bool JsonValue::has(const std::string& key) const {
    if (!isObject()) {
        return false;
    }
    const auto& obj = std::get<std::map<std::string, JsonValue>>(impl_->value);
    return obj.find(key) != obj.end();
}

std::vector<std::string> JsonValue::keys() const {
    std::vector<std::string> result;
    if (isObject()) {
        const auto& obj = std::get<std::map<std::string, JsonValue>>(impl_->value);
        for (const auto& [key, _] : obj) {
            result.push_back(key);
        }
    }
    return result;
}

std::string JsonValue::toJsonString() const {
    std::ostringstream oss;

    switch (getType()) {
        case Type::Null:
            oss << "null";
            break;

        case Type::Bool:
            oss << (getBool() ? "true" : "false");
            break;

        case Type::Number:
            if (std::holds_alternative<int64_t>(impl_->value)) {
                oss << getInt();
            } else {
                oss << getDouble();
            }
            break;

        case Type::String: {
            oss << "\"";
            const auto& str = getString();
            for (char c : str) {
                switch (c) {
                    case '"':
                        oss << "\\\"";
                        break;
                    case '\\':
                        oss << "\\\\";
                        break;
                    case '\n':
                        oss << "\\n";
                        break;
                    case '\r':
                        oss << "\\r";
                        break;
                    case '\t':
                        oss << "\\t";
                        break;
                    default:
                        oss << c;
                }
            }
            oss << "\"";
            break;
        }

        case Type::Array: {
            oss << "[";
            const auto& arr = std::get<std::vector<JsonValue>>(impl_->value);
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) oss << ",";
                oss << arr[i].toJsonString();
            }
            oss << "]";
            break;
        }

        case Type::Object: {
            oss << "{";
            const auto& obj = std::get<std::map<std::string, JsonValue>>(impl_->value);
            bool first = true;
            for (const auto& [key, value] : obj) {
                if (!first) oss << ",";
                first = false;
                oss << "\"" << key << "\":" << value.toJsonString();
            }
            oss << "}";
            break;
        }
    }

    return oss.str();
}

JsonValue JsonValue::fromJsonString(const std::string& json) {
    // Simplified JSON parser
    // In production, use a proper JSON library like nlohmann/json
    size_t pos = 0;

    std::function<void()> skipWhitespace = [&]() {
        while (pos < json.size() && std::isspace(json[pos])) {
            ++pos;
        }
    };

    std::function<JsonValue()> parseValue;

    auto parseString = [&]() -> std::string {
        if (json[pos] != '"') {
            throw std::runtime_error("Expected '\"'");
        }
        ++pos;

        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case 'n':
                        result += '\n';
                        break;
                    case 'r':
                        result += '\r';
                        break;
                    case 't':
                        result += '\t';
                        break;
                    case '"':
                        result += '"';
                        break;
                    case '\\':
                        result += '\\';
                        break;
                    default:
                        result += json[pos];
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }

        if (pos >= json.size()) {
            throw std::runtime_error("Unterminated string");
        }
        ++pos;  // Skip closing quote

        return result;
    };

    auto parseNumber = [&]() -> JsonValue {
        size_t start = pos;
        bool isDouble = false;

        if (json[pos] == '-') ++pos;

        while (pos < json.size() && std::isdigit(json[pos])) {
            ++pos;
        }

        if (pos < json.size() && json[pos] == '.') {
            isDouble = true;
            ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) {
                ++pos;
            }
        }

        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            isDouble = true;
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) {
                ++pos;
            }
            while (pos < json.size() && std::isdigit(json[pos])) {
                ++pos;
            }
        }

        std::string numStr = json.substr(start, pos - start);
        if (isDouble) {
            return JsonValue(std::stod(numStr));
        } else {
            return JsonValue(std::stoll(numStr));
        }
    };

    auto parseArray = [&]() -> JsonValue {
        JsonValue arr = JsonValue::makeArray();
        ++pos;  // Skip '['

        skipWhitespace();
        if (json[pos] == ']') {
            ++pos;
            return arr;
        }

        while (true) {
            arr.pushBack(parseValue());
            skipWhitespace();

            if (json[pos] == ']') {
                ++pos;
                break;
            }

            if (json[pos] != ',') {
                throw std::runtime_error("Expected ',' or ']'");
            }
            ++pos;
            skipWhitespace();
        }

        return arr;
    };

    auto parseObject = [&]() -> JsonValue {
        JsonValue obj = JsonValue::makeObject();
        ++pos;  // Skip '{'

        skipWhitespace();
        if (json[pos] == '}') {
            ++pos;
            return obj;
        }

        while (true) {
            skipWhitespace();
            std::string key = parseString();
            skipWhitespace();

            if (json[pos] != ':') {
                throw std::runtime_error("Expected ':'");
            }
            ++pos;

            skipWhitespace();
            obj.set(key, parseValue());
            skipWhitespace();

            if (json[pos] == '}') {
                ++pos;
                break;
            }

            if (json[pos] != ',') {
                throw std::runtime_error("Expected ',' or '}'");
            }
            ++pos;
        }

        return obj;
    };

    parseValue = [&]() -> JsonValue {
        skipWhitespace();

        if (pos >= json.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }

        if (json[pos] == 'n') {
            if (json.substr(pos, 4) == "null") {
                pos += 4;
                return JsonValue();
            }
            throw std::runtime_error("Invalid JSON");
        }

        if (json[pos] == 't') {
            if (json.substr(pos, 4) == "true") {
                pos += 4;
                return JsonValue(true);
            }
            throw std::runtime_error("Invalid JSON");
        }

        if (json[pos] == 'f') {
            if (json.substr(pos, 5) == "false") {
                pos += 5;
                return JsonValue(false);
            }
            throw std::runtime_error("Invalid JSON");
        }

        if (json[pos] == '"') {
            return JsonValue(parseString());
        }

        if (json[pos] == '[') {
            return parseArray();
        }

        if (json[pos] == '{') {
            return parseObject();
        }

        if (json[pos] == '-' || std::isdigit(json[pos])) {
            return parseNumber();
        }

        throw std::runtime_error("Invalid JSON");
    };

    return parseValue();
}

JsonValue JsonValue::makeArray() {
    JsonValue val;
    val.impl_->value = std::vector<JsonValue>();
    return val;
}

JsonValue JsonValue::makeObject() {
    JsonValue val;
    val.impl_->value = std::map<std::string, JsonValue>();
    return val;
}

// ============================================================================
// RpcServer Implementation
// ============================================================================

struct RpcServer::Impl {
    std::string bindAddress;
    uint16_t port;
    std::map<std::string, RpcMethod> methods;
    std::atomic<bool> running;
    std::thread serverThread;

    // Authentication
    bool authEnabled;
    std::string username;
    std::string password;
    std::unique_ptr<AuthenticationManager> authManager;
};

RpcServer::RpcServer(const std::string& bindAddress, uint16_t port)
    : impl_(std::make_unique<Impl>()) {
    impl_->bindAddress = bindAddress;
    impl_->port = port;
    impl_->running = false;
    impl_->authEnabled = false;

    // Initialize authentication manager
    AuthenticationManager::Config authConfig;
    authConfig.requireAuth = false;  // Disabled by default
    impl_->authManager = std::make_unique<AuthenticationManager>(authConfig);
}

RpcServer::~RpcServer() {
    stop();
}

void RpcServer::registerMethod(const std::string& name, RpcMethod method) {
    impl_->methods[name] = method;
    spdlog::debug("Registered RPC method: {}", name);
}

bool RpcServer::start() {
    if (impl_->running) {
        spdlog::warn("RPC server already running");
        return true;
    }

    impl_->running = true;

    // In a full implementation, this would use Boost.Beast or similar
    // to create an HTTP server. For now, we just log the startup.
    spdlog::info("RPC server started on {}:{}", impl_->bindAddress, impl_->port);

    return true;
}

void RpcServer::stop() {
    if (!impl_->running) {
        return;
    }

    spdlog::info("Stopping RPC server...");
    impl_->running = false;

    if (impl_->serverThread.joinable()) {
        impl_->serverThread.join();
    }

    spdlog::info("RPC server stopped");
}

bool RpcServer::isRunning() const {
    return impl_->running;
}

std::string RpcServer::processRequest(const std::string& request) {
    try {
        JsonValue req = JsonValue::fromJsonString(request);
        JsonValue response = handleCall(req);
        return response.toJsonString();
    } catch (const std::exception& e) {
        spdlog::error("Error processing RPC request: {}", e.what());
        JsonValue errorResp =
            createErrorResponse(JsonValue(), ErrorCode::PARSE_ERROR, e.what());
        return errorResp.toJsonString();
    }
}

void RpcServer::setAuth(const std::string& username, const std::string& password) {
    impl_->username = username;
    impl_->password = password;

    // Add user to authentication manager
    if (!impl_->authManager->addUser(username, password)) {
        spdlog::error("Failed to add RPC user: {}", username);
    } else {
        spdlog::info("RPC user '{}' configured", username);
    }
}

void RpcServer::setAuthEnabled(bool enabled) {
    impl_->authEnabled = enabled;
    spdlog::info("RPC authentication {}", enabled ? "enabled" : "disabled");
}

std::string RpcServer::processAuthenticatedRequest(const std::string& request,
                                                   const std::string& authToken,
                                                   const std::string& clientIP) {
    // Check if authentication is required
    if (!impl_->authEnabled) {
        return processRequest(request);
    }

    // Check if IP is banned
    if (!clientIP.empty() && impl_->authManager->isBanned(clientIP)) {
        JsonValue errorResp = createErrorResponse(
            JsonValue(),
            ErrorCode::RPC_AUTH_IP_BANNED,
            "IP address is banned due to repeated authentication failures");
        return errorResp.toJsonString();
    }

    // Validate token
    if (authToken.empty() || !impl_->authManager->validateToken(authToken)) {
        JsonValue errorResp = createErrorResponse(
            JsonValue(),
            ErrorCode::RPC_AUTH_REQUIRED,
            "Authentication required. Please provide a valid session token.");
        return errorResp.toJsonString();
    }

    // Process the authenticated request
    return processRequest(request);
}

AuthenticationManager& RpcServer::getAuthManager() {
    return *impl_->authManager;
}

const AuthenticationManager& RpcServer::getAuthManager() const {
    return *impl_->authManager;
}

JsonValue RpcServer::handleCall(const JsonValue& request) {
    // Validate JSON-RPC 2.0 request
    if (!request.isObject()) {
        return createErrorResponse(JsonValue(), ErrorCode::INVALID_REQUEST,
                                   "Request must be an object");
    }

    if (!request.has("jsonrpc") || request["jsonrpc"].getString() != "2.0") {
        return createErrorResponse(JsonValue(), ErrorCode::INVALID_REQUEST,
                                   "Invalid JSON-RPC version");
    }

    if (!request.has("method") || !request["method"].isString()) {
        return createErrorResponse(JsonValue(), ErrorCode::INVALID_REQUEST,
                                   "Missing or invalid method");
    }

    JsonValue id = request.has("id") ? request["id"] : JsonValue();
    std::string method = request["method"].getString();
    JsonValue params = request.has("params") ? request["params"] : JsonValue::makeArray();

    // Find method handler
    auto it = impl_->methods.find(method);
    if (it == impl_->methods.end()) {
        return createErrorResponse(id, ErrorCode::METHOD_NOT_FOUND, "Method not found");
    }

    try {
        // Call method handler
        JsonValue result = it->second(params);
        return createSuccessResponse(id, result);
    } catch (const std::exception& e) {
        spdlog::error("RPC method {} failed: {}", method, e.what());
        return createErrorResponse(id, ErrorCode::INTERNAL_ERROR, e.what());
    }
}

JsonValue RpcServer::createErrorResponse(const JsonValue& id, int code,
                                          const std::string& message) {
    JsonValue error = JsonValue::makeObject();
    error.set("code", JsonValue(static_cast<int64_t>(code)));
    error.set("message", JsonValue(message));

    JsonValue response = JsonValue::makeObject();
    response.set("jsonrpc", JsonValue("2.0"));
    response.set("error", error);
    response.set("id", id);

    return response;
}

JsonValue RpcServer::createSuccessResponse(const JsonValue& id, const JsonValue& result) {
    JsonValue response = JsonValue::makeObject();
    response.set("jsonrpc", JsonValue("2.0"));
    response.set("result", result);
    response.set("id", id);

    return response;
}

}  // namespace rpc
}  // namespace ubuntu
