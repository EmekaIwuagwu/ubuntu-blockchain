#pragma once

#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/signatures.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ubuntu {
namespace core {

/**
 * @brief Transaction output point (reference to a UTXO)
 */
struct TxOutpoint {
    crypto::Hash256 txHash;  // Transaction hash containing the output
    uint32_t vout;            // Output index in that transaction

    TxOutpoint() : vout(0) {}
    TxOutpoint(const crypto::Hash256& hash, uint32_t index)
        : txHash(hash), vout(index) {}

    // Serialization
    std::vector<uint8_t> serialize() const;
    static TxOutpoint deserialize(std::span<const uint8_t> data);

    // Comparison operators (for use in maps/sets)
    bool operator==(const TxOutpoint& other) const;
    bool operator!=(const TxOutpoint& other) const;
    bool operator<(const TxOutpoint& other) const;

    // String representation
    std::string toString() const;
};

/**
 * @brief Transaction input
 */
struct TxInput {
    TxOutpoint previousOutput;  // Reference to UTXO being spent
    std::vector<uint8_t> scriptSig;  // Signature script (unlocking script)
    uint32_t sequence;  // Sequence number (for RBF and timelock)

    TxInput() : sequence(0xFFFFFFFF) {}

    // Serialization
    std::vector<uint8_t> serialize() const;
    static TxInput deserialize(std::span<const uint8_t> data, size_t& offset);

    // Check if this is a coinbase input
    bool isCoinbase() const;
};

/**
 * @brief Transaction output
 */
struct TxOutput {
    uint64_t value;  // Amount in satoshis (1 UBU = 10^8 satoshis)
    std::vector<uint8_t> scriptPubKey;  // Locking script (public key script)

    TxOutput() : value(0) {}
    TxOutput(uint64_t val, const std::vector<uint8_t>& script)
        : value(val), scriptPubKey(script) {}

    // Serialization
    std::vector<uint8_t> serialize() const;
    static TxOutput deserialize(std::span<const uint8_t> data, size_t& offset);

    // Check if this is a dust output
    bool isDust(uint64_t dustThreshold = 546) const;
};

/**
 * @brief Transaction
 */
class Transaction {
public:
    static constexpr uint32_t CURRENT_VERSION = 1;
    static constexpr uint64_t COIN = 100000000;  // 1 UBU = 10^8 satoshis

    uint32_t version;
    std::vector<TxInput> inputs;
    std::vector<TxOutput> outputs;
    uint32_t lockTime;  // Block height or timestamp

    Transaction() : version(CURRENT_VERSION), lockTime(0) {}

    // Hashing
    crypto::Hash256 getHash() const;
    crypto::Hash256 getSignatureHash(size_t inputIndex) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static Transaction deserialize(std::span<const uint8_t> data);

    // Validation
    bool isCoinbase() const;
    bool isNull() const;
    size_t getSize() const;
    uint64_t getTotalOutput() const;

    // Fee calculation (requires UTXO set to get input values)
    std::optional<uint64_t> getFee(const std::vector<uint64_t>& inputValues) const;

    // String representation
    std::string toString() const;
};

/**
 * @brief UTXO (Unspent Transaction Output)
 */
struct UTXO {
    TxOutpoint outpoint;
    TxOutput output;
    uint32_t height;      // Block height when created
    bool isCoinbase;      // Is this from a coinbase transaction

    UTXO() : height(0), isCoinbase(false) {}

    UTXO(const TxOutpoint& op, const TxOutput& out,
         uint32_t h, bool coinbase)
        : outpoint(op), output(out), height(h), isCoinbase(coinbase) {}

    // Maturity check (coinbase outputs require 100 confirmations)
    bool isMature(uint32_t currentHeight) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static UTXO deserialize(std::span<const uint8_t> data);
};

/**
 * @brief Script utilities
 */
namespace Script {

// Script opcodes (subset of Bitcoin opcodes)
enum Opcode : uint8_t {
    OP_DUP = 0x76,
    OP_HASH160 = 0xa9,
    OP_EQUALVERIFY = 0x88,
    OP_CHECKSIG = 0xac,
    OP_CHECKMULTISIG = 0xae,
    OP_RETURN = 0x6a,
};

/**
 * @brief Create a P2PKH (Pay to Public Key Hash) script
 *
 * Standard script: OP_DUP OP_HASH160 <pubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
 */
std::vector<uint8_t> createP2PKH(const crypto::Hash160& pubKeyHash);

/**
 * @brief Create a P2SH (Pay to Script Hash) script
 *
 * Standard script: OP_HASH160 <scriptHash> OP_EQUAL
 */
std::vector<uint8_t> createP2SH(const crypto::Hash160& scriptHash);

/**
 * @brief Extract public key hash from P2PKH script
 */
std::optional<crypto::Hash160> extractP2PKH(std::span<const uint8_t> script);

/**
 * @brief Check if script is P2PKH
 */
bool isP2PKH(std::span<const uint8_t> script);

/**
 * @brief Check if script is P2SH
 */
bool isP2SH(std::span<const uint8_t> script);

/**
 * @brief Execute script (simplified script interpreter)
 *
 * Full implementation would include complete Bitcoin script execution.
 */
bool executeScript(std::span<const uint8_t> scriptSig,
                   std::span<const uint8_t> scriptPubKey,
                   const Transaction& tx,
                   size_t inputIndex);

}  // namespace Script

}  // namespace core
}  // namespace ubuntu

// Hash function for std::unordered_map/set support
namespace std {
template <>
struct hash<ubuntu::core::TxOutpoint> {
    size_t operator()(const ubuntu::core::TxOutpoint& outpoint) const noexcept {
        size_t h1 = std::hash<ubuntu::crypto::Hash256>{}(outpoint.txHash);
        size_t h2 = std::hash<uint32_t>{}(outpoint.vout);
        return h1 ^ (h2 << 1);
    }
};
}  // namespace std
