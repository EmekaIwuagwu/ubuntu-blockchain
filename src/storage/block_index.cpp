#include "ubuntu/storage/block_index.h"

#include <spdlog/spdlog.h>

namespace ubuntu {
namespace storage {

// ============================================================================
// BlockStorage Implementation
// ============================================================================

BlockStorage::BlockStorage(std::shared_ptr<Database> db) : db_(db) {}

BlockStorage::~BlockStorage() = default;

bool BlockStorage::storeBlock(const core::Block& block) {
    auto hash = block.calculateHash();
    auto data = serializeBlock(block);

    if (!db_->put(ColumnFamily::BLOCKS,
                  std::span<const uint8_t>(hash.begin(), hash.size()),
                  std::span<const uint8_t>(data.data(), data.size()))) {
        spdlog::error("Failed to store block {}", hash.toHex());
        return false;
    }

    spdlog::debug("Stored block {} ({} bytes)", hash.toHex(), data.size());
    return true;
}

std::optional<core::Block> BlockStorage::getBlock(const crypto::Hash256& hash) const {
    auto data = db_->get(ColumnFamily::BLOCKS,
                         std::span<const uint8_t>(hash.begin(), hash.size()));

    if (!data) {
        return std::nullopt;
    }

    return deserializeBlock(std::span<const uint8_t>(data->data(), data->size()));
}

std::optional<core::Block> BlockStorage::getBlockByHeight(uint32_t height) const {
    // Look up hash by height
    auto hash = getHashAtHeight(height);
    if (!hash) {
        return std::nullopt;
    }

    return getBlock(*hash);
}

bool BlockStorage::hasBlock(const crypto::Hash256& hash) const {
    return db_->exists(ColumnFamily::BLOCKS,
                       std::span<const uint8_t>(hash.begin(), hash.size()));
}

bool BlockStorage::deleteBlock(const crypto::Hash256& hash) {
    return db_->remove(ColumnFamily::BLOCKS,
                       std::span<const uint8_t>(hash.begin(), hash.size()));
}

bool BlockStorage::storeBlockHeader(const core::BlockHeader& header) {
    auto hash = header.getHash();
    auto data = serializeHeader(header);

    if (!db_->put(ColumnFamily::BLOCKS,
                  std::span<const uint8_t>(hash.begin(), hash.size()),
                  std::span<const uint8_t>(data.data(), data.size()))) {
        spdlog::error("Failed to store block header {}", hash.toHex());
        return false;
    }

    return true;
}

std::optional<core::BlockHeader> BlockStorage::getBlockHeader(
    const crypto::Hash256& hash) const {

    auto data = db_->get(ColumnFamily::BLOCKS,
                         std::span<const uint8_t>(hash.begin(), hash.size()));

    if (!data) {
        return std::nullopt;
    }

    // Try to deserialize as header first
    return deserializeHeader(std::span<const uint8_t>(data->data(), data->size()));
}

bool BlockStorage::setHeightIndex(uint32_t height, const crypto::Hash256& hash) {
    // Key: 4-byte height (big-endian for proper ordering)
    std::vector<uint8_t> key(4);
    key[0] = (height >> 24) & 0xFF;
    key[1] = (height >> 16) & 0xFF;
    key[2] = (height >> 8) & 0xFF;
    key[3] = height & 0xFF;

    return db_->put(ColumnFamily::BLOCK_INDEX,
                    std::span<const uint8_t>(key.data(), key.size()),
                    std::span<const uint8_t>(hash.begin(), hash.size()));
}

std::optional<crypto::Hash256> BlockStorage::getHashAtHeight(uint32_t height) const {
    // Key: 4-byte height (big-endian)
    std::vector<uint8_t> key(4);
    key[0] = (height >> 24) & 0xFF;
    key[1] = (height >> 16) & 0xFF;
    key[2] = (height >> 8) & 0xFF;
    key[3] = height & 0xFF;

    auto data = db_->get(ColumnFamily::BLOCK_INDEX,
                         std::span<const uint8_t>(key.data(), key.size()));

    if (!data || data->size() != 32) {
        return std::nullopt;
    }

    return crypto::Hash256(std::span<const uint8_t>(data->data(), 32));
}

uint32_t BlockStorage::getHeight() const {
    // Load from chain state
    auto state = loadChainState();
    if (state) {
        return state->height;
    }
    return 0;
}

bool BlockStorage::storeChainState(const core::ChainState& state) {
    // Serialize chain state
    std::vector<uint8_t> data;

    // Best block hash (32 bytes)
    data.insert(data.end(), state.bestBlockHash.begin(), state.bestBlockHash.end());

    // Height (4 bytes)
    data.push_back(state.height & 0xFF);
    data.push_back((state.height >> 8) & 0xFF);
    data.push_back((state.height >> 16) & 0xFF);
    data.push_back((state.height >> 24) & 0xFF);

    // Total work (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back((state.totalWork >> (i * 8)) & 0xFF);
    }

    // Timestamp (4 bytes)
    data.push_back(state.timestamp & 0xFF);
    data.push_back((state.timestamp >> 8) & 0xFF);
    data.push_back((state.timestamp >> 16) & 0xFF);
    data.push_back((state.timestamp >> 24) & 0xFF);

    // Difficulty (4 bytes)
    data.push_back(state.difficulty & 0xFF);
    data.push_back((state.difficulty >> 8) & 0xFF);
    data.push_back((state.difficulty >> 16) & 0xFF);
    data.push_back((state.difficulty >> 24) & 0xFF);

    return db_->put(ColumnFamily::CHAIN_STATE, "state", std::string(data.begin(), data.end()));
}

std::optional<core::ChainState> BlockStorage::loadChainState() const {
    auto dataOpt = db_->get(ColumnFamily::CHAIN_STATE, "state");
    if (!dataOpt) {
        return std::nullopt;
    }

    std::vector<uint8_t> data(dataOpt->begin(), dataOpt->end());

    if (data.size() < 52) {  // Minimum size
        return std::nullopt;
    }

    core::ChainState state;

    size_t offset = 0;

    // Best block hash (32 bytes)
    state.bestBlockHash = crypto::Hash256(std::span<const uint8_t>(data.data() + offset, 32));
    offset += 32;

    // Height (4 bytes)
    state.height = data[offset] |
                   (data[offset + 1] << 8) |
                   (data[offset + 2] << 16) |
                   (data[offset + 3] << 24);
    offset += 4;

    // Total work (8 bytes)
    state.totalWork = 0;
    for (int i = 0; i < 8; ++i) {
        state.totalWork |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Timestamp (4 bytes)
    state.timestamp = data[offset] |
                      (data[offset + 1] << 8) |
                      (data[offset + 2] << 16) |
                      (data[offset + 3] << 24);
    offset += 4;

    // Difficulty (4 bytes)
    state.difficulty = data[offset] |
                       (data[offset + 1] << 8) |
                       (data[offset + 2] << 16) |
                       (data[offset + 3] << 24);

    return state;
}

uint64_t BlockStorage::getBlockCount() const {
    // This is approximate - would need to iterate through blocks
    return static_cast<uint64_t>(getHeight()) + 1;
}

uint64_t BlockStorage::getStorageSize() const {
    return db_->getApproximateSize();
}

std::vector<uint8_t> BlockStorage::serializeBlock(const core::Block& block) const {
    return block.serialize();
}

std::optional<core::Block> BlockStorage::deserializeBlock(
    std::span<const uint8_t> data) const {
    try {
        return core::Block::deserialize(data);
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize block: {}", e.what());
        return std::nullopt;
    }
}

std::vector<uint8_t> BlockStorage::serializeHeader(const core::BlockHeader& header) const {
    return header.serialize();
}

std::optional<core::BlockHeader> BlockStorage::deserializeHeader(
    std::span<const uint8_t> data) const {
    // Simplified header deserialization
    if (data.size() < 80) {  // Block header is 80 bytes
        return std::nullopt;
    }

    core::BlockHeader header;
    size_t offset = 0;

    // Version (4 bytes)
    header.version = data[offset] |
                     (data[offset + 1] << 8) |
                     (data[offset + 2] << 16) |
                     (data[offset + 3] << 24);
    offset += 4;

    // Previous block hash (32 bytes)
    header.previousBlockHash = crypto::Hash256(std::span<const uint8_t>(data.data() + offset, 32));
    offset += 32;

    // Merkle root (32 bytes)
    header.merkleRoot = crypto::Hash256(std::span<const uint8_t>(data.data() + offset, 32));
    offset += 32;

    // Timestamp (4 bytes)
    header.timestamp = data[offset] |
                       (data[offset + 1] << 8) |
                       (data[offset + 2] << 16) |
                       (data[offset + 3] << 24);
    offset += 4;

    // Difficulty (4 bytes)
    header.difficulty = data[offset] |
                        (data[offset + 1] << 8) |
                        (data[offset + 2] << 16) |
                        (data[offset + 3] << 24);
    offset += 4;

    // Nonce (4 bytes)
    header.nonce = data[offset] |
                   (data[offset + 1] << 8) |
                   (data[offset + 2] << 16) |
                   (data[offset + 3] << 24);

    return header;
}

}  // namespace storage
}  // namespace ubuntu
