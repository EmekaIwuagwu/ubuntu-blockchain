#include "ubuntu/core/transaction.h"

#include <algorithm>
#include <sstream>

namespace ubuntu {
namespace core {

// ============================================================================
// TxOutpoint Implementation
// ============================================================================

std::vector<uint8_t> TxOutpoint::serialize() const {
    std::vector<uint8_t> result;
    // Append tx hash
    result.insert(result.end(), txHash.begin(), txHash.end());
    // Append vout (little-endian)
    result.push_back(vout & 0xFF);
    result.push_back((vout >> 8) & 0xFF);
    result.push_back((vout >> 16) & 0xFF);
    result.push_back((vout >> 24) & 0xFF);
    return result;
}

TxOutpoint TxOutpoint::deserialize(std::span<const uint8_t> data) {
    if (data.size() < 36) {
        throw std::runtime_error("TxOutpoint::deserialize: insufficient data");
    }

    crypto::Hash256 hash(std::span<const uint8_t>(data.data(), 32));

    uint32_t vout = data[32] |
                    (data[33] << 8) |
                    (data[34] << 16) |
                    (data[35] << 24);

    return TxOutpoint(hash, vout);
}

bool TxOutpoint::operator==(const TxOutpoint& other) const {
    return txHash == other.txHash && vout == other.vout;
}

bool TxOutpoint::operator!=(const TxOutpoint& other) const {
    return !(*this == other);
}

bool TxOutpoint::operator<(const TxOutpoint& other) const {
    if (txHash != other.txHash) {
        return txHash < other.txHash;
    }
    return vout < other.vout;
}

std::string TxOutpoint::toString() const {
    std::ostringstream oss;
    oss << txHash.toHex() << ":" << vout;
    return oss.str();
}

// ============================================================================
// TxInput Implementation
// ============================================================================

std::vector<uint8_t> TxInput::serialize() const {
    std::vector<uint8_t> result;

    // Serialize previous output
    auto outpointData = previousOutput.serialize();
    result.insert(result.end(), outpointData.begin(), outpointData.end());

    // Serialize scriptSig (with length prefix)
    size_t scriptLen = scriptSig.size();
    result.push_back(static_cast<uint8_t>(scriptLen));
    result.insert(result.end(), scriptSig.begin(), scriptSig.end());

    // Serialize sequence
    result.push_back(sequence & 0xFF);
    result.push_back((sequence >> 8) & 0xFF);
    result.push_back((sequence >> 16) & 0xFF);
    result.push_back((sequence >> 24) & 0xFF);

    return result;
}

TxInput TxInput::deserialize(std::span<const uint8_t> data, size_t& offset) {
    TxInput input;

    // Deserialize outpoint (36 bytes)
    input.previousOutput = TxOutpoint::deserialize(
        std::span<const uint8_t>(data.data() + offset, 36));
    offset += 36;

    // Deserialize scriptSig
    uint8_t scriptLen = data[offset++];
    input.scriptSig.resize(scriptLen);
    std::copy(data.begin() + offset, data.begin() + offset + scriptLen,
              input.scriptSig.begin());
    offset += scriptLen;

    // Deserialize sequence
    input.sequence = data[offset] |
                     (data[offset + 1] << 8) |
                     (data[offset + 2] << 16) |
                     (data[offset + 3] << 24);
    offset += 4;

    return input;
}

bool TxInput::isCoinbase() const {
    return previousOutput.txHash == crypto::Hash256::zero() &&
           previousOutput.vout == 0xFFFFFFFF;
}

// ============================================================================
// TxOutput Implementation
// ============================================================================

std::vector<uint8_t> TxOutput::serialize() const {
    std::vector<uint8_t> result;

    // Serialize value (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back((value >> (i * 8)) & 0xFF);
    }

    // Serialize scriptPubKey (with length prefix)
    size_t scriptLen = scriptPubKey.size();
    result.push_back(static_cast<uint8_t>(scriptLen));
    result.insert(result.end(), scriptPubKey.begin(), scriptPubKey.end());

    return result;
}

TxOutput TxOutput::deserialize(std::span<const uint8_t> data, size_t& offset) {
    TxOutput output;

    // Deserialize value
    output.value = 0;
    for (int i = 0; i < 8; ++i) {
        output.value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize scriptPubKey
    uint8_t scriptLen = data[offset++];
    output.scriptPubKey.resize(scriptLen);
    std::copy(data.begin() + offset, data.begin() + offset + scriptLen,
              output.scriptPubKey.begin());
    offset += scriptLen;

    return output;
}

bool TxOutput::isDust(uint64_t dustThreshold) const {
    return value < dustThreshold;
}

// ============================================================================
// Transaction Implementation
// ============================================================================

crypto::Hash256 Transaction::getHash() const {
    auto serialized = serialize();
    return crypto::sha256d(std::span<const uint8_t>(serialized.data(),
                                                      serialized.size()));
}

crypto::Hash256 Transaction::getSignatureHash(size_t inputIndex) const {
    // Simplified signature hash computation
    // Full implementation would follow BIP-143 for SegWit
    if (inputIndex >= inputs.size()) {
        throw std::out_of_range("Invalid input index");
    }

    auto serialized = serialize();
    return crypto::sha256d(std::span<const uint8_t>(serialized.data(),
                                                      serialized.size()));
}

std::vector<uint8_t> Transaction::serialize() const {
    std::vector<uint8_t> result;

    // Version (4 bytes)
    result.push_back(version & 0xFF);
    result.push_back((version >> 8) & 0xFF);
    result.push_back((version >> 16) & 0xFF);
    result.push_back((version >> 24) & 0xFF);

    // Input count
    result.push_back(static_cast<uint8_t>(inputs.size()));

    // Inputs
    for (const auto& input : inputs) {
        auto inputData = input.serialize();
        result.insert(result.end(), inputData.begin(), inputData.end());
    }

    // Output count
    result.push_back(static_cast<uint8_t>(outputs.size()));

    // Outputs
    for (const auto& output : outputs) {
        auto outputData = output.serialize();
        result.insert(result.end(), outputData.begin(), outputData.end());
    }

    // Locktime (4 bytes)
    result.push_back(lockTime & 0xFF);
    result.push_back((lockTime >> 8) & 0xFF);
    result.push_back((lockTime >> 16) & 0xFF);
    result.push_back((lockTime >> 24) & 0xFF);

    return result;
}

Transaction Transaction::deserialize(std::span<const uint8_t> data) {
    Transaction tx;
    size_t offset = 0;

    // Version
    tx.version = data[offset] |
                 (data[offset + 1] << 8) |
                 (data[offset + 2] << 16) |
                 (data[offset + 3] << 24);
    offset += 4;

    // Input count
    uint8_t inputCount = data[offset++];

    // Inputs
    for (size_t i = 0; i < inputCount; ++i) {
        tx.inputs.push_back(TxInput::deserialize(data, offset));
    }

    // Output count
    uint8_t outputCount = data[offset++];

    // Outputs
    for (size_t i = 0; i < outputCount; ++i) {
        tx.outputs.push_back(TxOutput::deserialize(data, offset));
    }

    // Locktime
    tx.lockTime = data[offset] |
                  (data[offset + 1] << 8) |
                  (data[offset + 2] << 16) |
                  (data[offset + 3] << 24);

    return tx;
}

bool Transaction::isCoinbase() const {
    return inputs.size() == 1 && inputs[0].isCoinbase();
}

bool Transaction::isNull() const {
    return inputs.empty() && outputs.empty();
}

size_t Transaction::getSize() const {
    return serialize().size();
}

uint64_t Transaction::getTotalOutput() const {
    uint64_t total = 0;
    for (const auto& output : outputs) {
        total += output.value;
    }
    return total;
}

std::optional<uint64_t> Transaction::getFee(
    const std::vector<uint64_t>& inputValues) const {

    if (inputValues.size() != inputs.size()) {
        return std::nullopt;
    }

    uint64_t totalInput = 0;
    for (uint64_t value : inputValues) {
        totalInput += value;
    }

    uint64_t totalOutput = getTotalOutput();

    if (totalInput < totalOutput) {
        return std::nullopt;  // Invalid: outputs exceed inputs
    }

    return totalInput - totalOutput;
}

std::string Transaction::toString() const {
    std::ostringstream oss;
    oss << "Transaction{" << std::endl;
    oss << "  hash: " << getHash().toHex() << std::endl;
    oss << "  version: " << version << std::endl;
    oss << "  inputs: " << inputs.size() << std::endl;
    oss << "  outputs: " << outputs.size() << std::endl;
    oss << "  lockTime: " << lockTime << std::endl;
    oss << "}";
    return oss.str();
}

// ============================================================================
// UTXO Implementation
// ============================================================================

bool UTXO::isMature(uint32_t currentHeight) const {
    if (!isCoinbase) {
        return true;  // Regular transactions are immediately mature
    }
    // Coinbase outputs require 100 confirmations
    return currentHeight >= height + 100;
}

std::vector<uint8_t> UTXO::serialize() const {
    std::vector<uint8_t> result;

    // Serialize outpoint
    auto outpointData = outpoint.serialize();
    result.insert(result.end(), outpointData.begin(), outpointData.end());

    // Serialize output
    auto outputData = output.serialize();
    result.insert(result.end(), outputData.begin(), outputData.end());

    // Serialize height
    result.push_back(height & 0xFF);
    result.push_back((height >> 8) & 0xFF);
    result.push_back((height >> 16) & 0xFF);
    result.push_back((height >> 24) & 0xFF);

    // Serialize isCoinbase flag
    result.push_back(isCoinbase ? 1 : 0);

    return result;
}

UTXO UTXO::deserialize(std::span<const uint8_t> data) {
    UTXO utxo;
    size_t offset = 0;

    // Deserialize outpoint
    utxo.outpoint = TxOutpoint::deserialize(data);
    offset += 36;

    // Deserialize output
    utxo.output = TxOutput::deserialize(data, offset);

    // Deserialize height
    utxo.height = data[offset] |
                  (data[offset + 1] << 8) |
                  (data[offset + 2] << 16) |
                  (data[offset + 3] << 24);
    offset += 4;

    // Deserialize isCoinbase flag
    utxo.isCoinbase = (data[offset] != 0);

    return utxo;
}

// ============================================================================
// Script Implementation
// ============================================================================

namespace Script {

std::vector<uint8_t> createP2PKH(const crypto::Hash160& pubKeyHash) {
    std::vector<uint8_t> script;
    script.push_back(OP_DUP);
    script.push_back(OP_HASH160);
    script.push_back(20);  // Hash160 size
    script.insert(script.end(), pubKeyHash.begin(), pubKeyHash.end());
    script.push_back(OP_EQUALVERIFY);
    script.push_back(OP_CHECKSIG);
    return script;
}

std::vector<uint8_t> createP2SH(const crypto::Hash160& scriptHash) {
    std::vector<uint8_t> script;
    script.push_back(OP_HASH160);
    script.push_back(20);  // Hash160 size
    script.insert(script.end(), scriptHash.begin(), scriptHash.end());
    script.push_back(OP_EQUALVERIFY);
    return script;
}

std::optional<crypto::Hash160> extractP2PKH(std::span<const uint8_t> script) {
    if (script.size() != 25) return std::nullopt;
    if (script[0] != OP_DUP) return std::nullopt;
    if (script[1] != OP_HASH160) return std::nullopt;
    if (script[2] != 20) return std::nullopt;
    if (script[23] != OP_EQUALVERIFY) return std::nullopt;
    if (script[24] != OP_CHECKSIG) return std::nullopt;

    return crypto::Hash160(std::span<const uint8_t>(script.data() + 3, 20));
}

bool isP2PKH(std::span<const uint8_t> script) {
    return extractP2PKH(script).has_value();
}

bool isP2SH(std::span<const uint8_t> script) {
    return script.size() == 23 &&
           script[0] == OP_HASH160 &&
           script[1] == 20 &&
           script[22] == OP_EQUALVERIFY;
}

bool executeScript(std::span<const uint8_t> scriptSig,
                   std::span<const uint8_t> scriptPubKey,
                   const Transaction& tx,
                   size_t inputIndex) {
    // Simplified script execution
    // Full implementation would include complete Bitcoin script interpreter
    // For now, just validate P2PKH scripts

    if (!isP2PKH(scriptPubKey)) {
        return false;  // Only P2PKH supported in this simplified version
    }

    // Extract signature and public key from scriptSig
    // Expected format: <signature> <pubkey>
    if (scriptSig.size() < 65) {
        return false;
    }

    // This is a placeholder - full implementation would:
    // 1. Parse signature and public key from scriptSig
    // 2. Compute signature hash
    // 3. Verify signature
    // 4. Hash public key and compare with scriptPubKey

    return true;  // Placeholder
}

}  // namespace Script

}  // namespace core
}  // namespace ubuntu
