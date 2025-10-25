#include "ubuntu/core/block.h"
#include <stdexcept>

namespace ubuntu {
namespace core {

crypto::Hash256 BlockHeader::getHash() const {
    return crypto::sha256d(serialize());
}

std::vector<uint8_t> BlockHeader::serialize() const {
    std::vector<uint8_t> result;
    // Simplified serialization
    return result;
}

crypto::Hash256 Block::calculateHash() const {
    return header.getHash();
}

crypto::Hash256 Block::calculateMerkleRoot() const {
    return crypto::Hash256::zero();
}

bool Block::validate() const {
    return true;
}

std::vector<uint8_t> Block::serialize() const {
    return header.serialize();
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
