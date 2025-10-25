#pragma once

#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"

#include <cstdint>
#include <vector>

namespace ubuntu {
namespace core {

struct BlockHeader {
    uint32_t version{1};
    crypto::Hash256 previousBlockHash;
    crypto::Hash256 merkleRoot;
    uint32_t timestamp{0};
    uint32_t difficulty{0};
    uint32_t nonce{0};
    uint32_t height{0};

    crypto::Hash256 getHash() const;
    std::vector<uint8_t> serialize() const;
};

class Block {
public:
    BlockHeader header;
    std::vector<Transaction> transactions;

    crypto::Hash256 calculateHash() const;
    crypto::Hash256 calculateMerkleRoot() const;
    bool validate() const;
    std::vector<uint8_t> serialize() const;
};

Block createGenesisBlock();

}  // namespace core
}  // namespace ubuntu
