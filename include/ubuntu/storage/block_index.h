#pragma once

#include "ubuntu/core/block.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/storage/database.h"

#include <memory>
#include <optional>

namespace ubuntu {
namespace storage {

/**
 * @brief Block storage manager
 *
 * Handles persistent storage of blocks and block metadata.
 */
class BlockStorage {
public:
    /**
     * @brief Construct block storage
     *
     * @param db Underlying database
     */
    explicit BlockStorage(std::shared_ptr<Database> db);
    ~BlockStorage();

    /**
     * @brief Store a block
     *
     * @param block Block to store
     * @return true if successful
     */
    bool storeBlock(const core::Block& block);

    /**
     * @brief Retrieve a block by hash
     *
     * @param hash Block hash
     * @return Block if found
     */
    std::optional<core::Block> getBlock(const crypto::Hash256& hash) const;

    /**
     * @brief Retrieve a block by height
     *
     * Uses the height index to find the block on the main chain.
     *
     * @param height Block height
     * @return Block if found
     */
    std::optional<core::Block> getBlockByHeight(uint32_t height) const;

    /**
     * @brief Check if a block exists
     *
     * @param hash Block hash
     * @return true if block exists
     */
    bool hasBlock(const crypto::Hash256& hash) const;

    /**
     * @brief Delete a block (for pruning)
     *
     * @param hash Block hash
     * @return true if successful
     */
    bool deleteBlock(const crypto::Hash256& hash);

    /**
     * @brief Store block header
     *
     * @param header Block header
     * @return true if successful
     */
    bool storeBlockHeader(const core::BlockHeader& header);

    /**
     * @brief Get block header
     *
     * @param hash Block hash
     * @return Block header if found
     */
    std::optional<core::BlockHeader> getBlockHeader(const crypto::Hash256& hash) const;

    /**
     * @brief Map height to block hash (for main chain)
     *
     * @param height Block height
     * @param hash Block hash
     * @return true if successful
     */
    bool setHeightIndex(uint32_t height, const crypto::Hash256& hash);

    /**
     * @brief Get block hash at height
     *
     * @param height Block height
     * @return Block hash if found
     */
    std::optional<crypto::Hash256> getHashAtHeight(uint32_t height) const;

    /**
     * @brief Get blockchain height
     *
     * @return Current height
     */
    uint32_t getHeight() const;

    /**
     * @brief Store chain state
     *
     * @param state Chain state
     * @return true if successful
     */
    bool storeChainState(const core::ChainState& state);

    /**
     * @brief Load chain state
     *
     * @return Chain state if found
     */
    std::optional<core::ChainState> loadChainState() const;

    /**
     * @brief Get total number of blocks stored
     *
     * @return Block count
     */
    uint64_t getBlockCount() const;

    /**
     * @brief Get total storage size
     *
     * @return Size in bytes
     */
    uint64_t getStorageSize() const;

private:
    std::shared_ptr<Database> db_;

    /**
     * @brief Serialize block to bytes
     */
    std::vector<uint8_t> serializeBlock(const core::Block& block) const;

    /**
     * @brief Deserialize block from bytes
     */
    std::optional<core::Block> deserializeBlock(std::span<const uint8_t> data) const;

    /**
     * @brief Serialize block header
     */
    std::vector<uint8_t> serializeHeader(const core::BlockHeader& header) const;

    /**
     * @brief Deserialize block header
     */
    std::optional<core::BlockHeader> deserializeHeader(std::span<const uint8_t> data) const;
};

}  // namespace storage
}  // namespace ubuntu
