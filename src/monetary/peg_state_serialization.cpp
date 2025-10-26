#include "ubuntu/monetary/peg_state.h"

#include <cstring>
#include <stdexcept>

namespace ubuntu {
namespace monetary {

namespace {

// Helper functions for binary serialization

template<typename T>
void write_value(std::vector<uint8_t>& buffer, const T& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template<typename T>
T read_value(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + sizeof(T) > end) {
        throw std::runtime_error("Buffer underflow during deserialization");
    }
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

void write_string(std::vector<uint8_t>& buffer, const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    write_value(buffer, length);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

std::string read_string(const uint8_t*& ptr, const uint8_t* end) {
    uint32_t length = read_value<uint32_t>(ptr, end);
    if (ptr + length > end) {
        throw std::runtime_error("String length exceeds buffer");
    }
    std::string str(reinterpret_cast<const char*>(ptr), length);
    ptr += length;
    return str;
}

// Helper to convert int128_t to bytes (as two int64_t values)
void write_int128(std::vector<uint8_t>& buffer, const int128_t& value) {
    // Split into high and low 64-bit parts
    int64_t low = static_cast<int64_t>(value & 0xFFFFFFFFFFFFFFFFLL);
    int64_t high = static_cast<int64_t>((value >> 64) & 0xFFFFFFFFFFFFFFFFLL);
    write_value(buffer, low);
    write_value(buffer, high);
}

int128_t read_int128(const uint8_t*& ptr, const uint8_t* end) {
    int64_t low = read_value<int64_t>(ptr, end);
    int64_t high = read_value<int64_t>(ptr, end);
    int128_t value = (static_cast<int128_t>(high) << 64) | static_cast<uint64_t>(low);
    return value;
}

}  // anonymous namespace

// ============================================================================
// PegState Serialization
// ============================================================================

std::vector<uint8_t> PegState::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(256);  // Pre-allocate reasonable size

    // Version number for future compatibility
    write_value(buffer, static_cast<uint32_t>(1));

    // Basic metadata
    write_value(buffer, epoch_id);
    write_value(buffer, timestamp);
    write_value(buffer, block_height);

    // Price and supply
    write_value(buffer, last_price_scaled);
    write_int128(buffer, last_supply);
    write_int128(buffer, last_delta);

    // Bond tracking
    write_int128(buffer, total_bond_debt);
    write_int128(buffer, bonds_issued_this_epoch);
    write_int128(buffer, bonds_redeemed_this_epoch);

    // PID controller state
    write_int128(buffer, integral);
    write_value(buffer, prev_error_scaled);

    // Diagnostics
    write_string(buffer, last_action);
    write_string(buffer, last_reason);
    write_value(buffer, circuit_breaker_triggered);

    return buffer;
}

PegState PegState::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Cannot deserialize empty buffer");
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    PegState state;

    // Read and validate version
    uint32_t version = read_value<uint32_t>(ptr, end);
    if (version != 1) {
        throw std::runtime_error("Unsupported PegState version: " + std::to_string(version));
    }

    // Basic metadata
    state.epoch_id = read_value<uint64_t>(ptr, end);
    state.timestamp = read_value<uint64_t>(ptr, end);
    state.block_height = read_value<uint64_t>(ptr, end);

    // Price and supply
    state.last_price_scaled = read_value<int64_t>(ptr, end);
    state.last_supply = read_int128(ptr, end);
    state.last_delta = read_int128(ptr, end);

    // Bond tracking
    state.total_bond_debt = read_int128(ptr, end);
    state.bonds_issued_this_epoch = read_int128(ptr, end);
    state.bonds_redeemed_this_epoch = read_int128(ptr, end);

    // PID controller state
    state.integral = read_int128(ptr, end);
    state.prev_error_scaled = read_value<int64_t>(ptr, end);

    // Diagnostics
    state.last_action = read_string(ptr, end);
    state.last_reason = read_string(ptr, end);
    state.circuit_breaker_triggered = read_value<bool>(ptr, end);

    return state;
}

// ============================================================================
// PegEvent Serialization
// ============================================================================

std::vector<uint8_t> PegEvent::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(128);

    // Version
    write_value(buffer, static_cast<uint32_t>(1));

    // Event data
    write_value(buffer, epoch_id);
    write_value(buffer, timestamp);
    write_value(buffer, block_height);
    write_value(buffer, price_scaled);
    write_int128(buffer, supply);
    write_int128(buffer, delta);
    write_string(buffer, action);
    write_string(buffer, reason);

    return buffer;
}

PegEvent PegEvent::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Cannot deserialize empty PegEvent buffer");
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    PegEvent event;

    // Read and validate version
    uint32_t version = read_value<uint32_t>(ptr, end);
    if (version != 1) {
        throw std::runtime_error("Unsupported PegEvent version: " + std::to_string(version));
    }

    // Event data
    event.epoch_id = read_value<uint64_t>(ptr, end);
    event.timestamp = read_value<uint64_t>(ptr, end);
    event.block_height = read_value<uint64_t>(ptr, end);
    event.price_scaled = read_value<int64_t>(ptr, end);
    event.supply = read_int128(ptr, end);
    event.delta = read_int128(ptr, end);
    event.action = read_string(ptr, end);
    event.reason = read_string(ptr, end);

    return event;
}

// ============================================================================
// BondState Serialization
// ============================================================================

std::vector<uint8_t> BondState::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(64);

    // Version
    write_value(buffer, static_cast<uint32_t>(1));

    // Bond data
    write_value(buffer, bond_id);
    write_int128(buffer, amount);
    write_value(buffer, issued_epoch);
    write_value(buffer, maturity_epoch);
    write_value(buffer, discount_rate_ppm);

    return buffer;
}

BondState BondState::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Cannot deserialize empty BondState buffer");
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    BondState bond;

    // Read and validate version
    uint32_t version = read_value<uint32_t>(ptr, end);
    if (version != 1) {
        throw std::runtime_error("Unsupported BondState version: " + std::to_string(version));
    }

    // Bond data
    bond.bond_id = read_value<uint64_t>(ptr, end);
    bond.amount = read_int128(ptr, end);
    bond.issued_epoch = read_value<uint64_t>(ptr, end);
    bond.maturity_epoch = read_value<uint64_t>(ptr, end);
    bond.discount_rate_ppm = read_value<int64_t>(ptr, end);

    return bond;
}

}  // namespace monetary
}  // namespace ubuntu
