#include "ubuntu/core/block.h"
#include "ubuntu/core/merkle.h"

#include <stdexcept>

namespace ubuntu {
namespace core {

crypto::Hash256 BlockHeader::getHash() const {
    return crypto::sha256d(serialize());
}

std::vector<uint8_t> BlockHeader::serialize() const {
    std::vector<uint8_t> result;

    // Version (4 bytes, little-endian)
    result.push_back(version & 0xFF);
    result.push_back((version >> 8) & 0xFF);
    result.push_back((version >> 16) & 0xFF);
    result.push_back((version >> 24) & 0xFF);

    // Previous block hash (32 bytes)
    result.insert(result.end(), previousBlockHash.begin(), previousBlockHash.end());

    // Merkle root (32 bytes)
    result.insert(result.end(), merkleRoot.begin(), merkleRoot.end());

    // Timestamp (4 bytes, little-endian)
    result.push_back(timestamp & 0xFF);
    result.push_back((timestamp >> 8) & 0xFF);
    result.push_back((timestamp >> 16) & 0xFF);
    result.push_back((timestamp >> 24) & 0xFF);

    // Difficulty (4 bytes, little-endian)
    result.push_back(difficulty & 0xFF);
    result.push_back((difficulty >> 8) & 0xFF);
    result.push_back((difficulty >> 16) & 0xFF);
    result.push_back((difficulty >> 24) & 0xFF);

    // Nonce (4 bytes, little-endian)
    result.push_back(nonce & 0xFF);
    result.push_back((nonce >> 8) & 0xFF);
    result.push_back((nonce >> 16) & 0xFF);
    result.push_back((nonce >> 24) & 0xFF);

    return result;
}

crypto::Hash256 Block::calculateHash() const {
    return header.getHash();
}

crypto::Hash256 Block::calculateMerkleRoot() const {
    if (transactions.empty()) {
        return crypto::Hash256::zero();
    }

    // Get all transaction hashes
    std::vector<crypto::Hash256> txHashes;
    txHashes.reserve(transactions.size());

    for (const auto& tx : transactions) {
        txHashes.push_back(tx.getHash());
    }

    // Compute Merkle root
    return MerkleTree::computeRoot(txHashes);
}

bool Block::validate() const {
    // Basic validation checks

    // 1. Must have at least one transaction (coinbase)
    if (transactions.empty()) {
        return false;
    }

    // 2. First transaction must be coinbase
    if (!transactions[0].isCoinbase()) {
        return false;
    }

    // 3. Only first transaction can be coinbase
    for (size_t i = 1; i < transactions.size(); ++i) {
        if (transactions[i].isCoinbase()) {
            return false;
        }
    }

    // 4. Verify Merkle root matches
    auto computedRoot = calculateMerkleRoot();
    if (computedRoot != header.merkleRoot) {
        return false;
    }

    // 5. Timestamp must not be too far in the future (2 hours)
    // Note: This would require current time, simplified for now

    return true;
}

std::vector<uint8_t> Block::serialize() const {
    std::vector<uint8_t> result;

    // Serialize header
    auto headerData = header.serialize();
    result.insert(result.end(), headerData.begin(), headerData.end());

    // Transaction count (varint - simplified to 1 byte for now)
    result.push_back(static_cast<uint8_t>(transactions.size()));

    // Serialize all transactions
    for (const auto& tx : transactions) {
        auto txData = tx.serialize();
        result.insert(result.end(), txData.begin(), txData.end());
    }

    return result;
}

Block createGenesisBlock() {
    Block genesis;
    genesis.header.version = 1;
    genesis.header.previousBlockHash = crypto::Hash256::zero();
    genesis.header.timestamp = 1704067200;
    genesis.header.difficulty = 0x1d00ffff;
    genesis.header.nonce = 0;
    genesis.header.height = 0;
    return genesis;
}

}  // namespace core
}  // namespace ubuntu
