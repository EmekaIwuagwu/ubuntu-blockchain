#pragma once

#include "ubuntu/consensus/chainparams.h"
#include "ubuntu/core/block.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ubuntu {
namespace core {

/**
 * @brief Block index entry (metadata for a block)
 */
struct BlockIndex {
    crypto::Hash256 hash;
    uint32_t height;
    uint64_t chainWork;  // Cumulative proof-of-work
    uint32_t timestamp;
    uint32_t difficulty;

    crypto::Hash256 previousHash;
    BlockIndex* previous;  // Pointer to previous block index
    std::vector<BlockIndex*> children;  // Pointers to child blocks (for forks)

    // Validation status
    bool valid;
    bool hasData;  // Whether full block data is available

    BlockIndex()
        : height(0), chainWork(0), timestamp(0), difficulty(0),
          previous(nullptr), valid(false), hasData(false) {}

    explicit BlockIndex(const Block& block)
        : hash(block.calculateHash()),
          height(block.header.height),
          chainWork(0),
          timestamp(block.header.timestamp),
          difficulty(block.header.difficulty),
          previousHash(block.header.previousBlockHash),
          previous(nullptr),
          valid(false),
          hasData(true) {}
};

/**
 * @brief Chain state tracker
 */
struct ChainState {
    crypto::Hash256 bestBlockHash;  // Current chain tip
    uint32_t height;                // Current chain height
    uint64_t totalWork;             // Total cumulative work
    uint32_t timestamp;             // Timestamp of best block
    uint32_t difficulty;            // Current difficulty

    ChainState()
        : height(0), totalWork(0), timestamp(0), difficulty(0) {}
};

/**
 * @brief Blockchain manager
 *
 * Manages the blockchain state, block validation, and chain selection.
 */
class Blockchain {
public:
    explicit Blockchain(const consensus::ChainParams& params);
    ~Blockchain();

    /**
     * @brief Initialize blockchain with genesis block
     *
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Add a new block to the blockchain
     *
     * Validates the block and adds it to the chain if valid.
     * May trigger a reorganization if the new block is on a better chain.
     *
     * @param block The block to add
     * @return true if block was accepted
     */
    bool addBlock(const Block& block);

    /**
     * @brief Get a block by hash
     *
     * @param hash Block hash
     * @return Block if found
     */
    std::optional<Block> getBlock(const crypto::Hash256& hash) const;

    /**
     * @brief Get a block by height
     *
     * @param height Block height
     * @return Block if found
     */
    std::optional<Block> getBlockByHeight(uint32_t height) const;

    /**
     * @brief Get the current chain tip (best block)
     *
     * @return Best block
     */
    std::optional<Block> getTip() const;

    /**
     * @brief Get current chain state
     *
     * @return Chain state
     */
    const ChainState& getState() const { return state_; }

    /**
     * @brief Get current blockchain height
     *
     * @return Height
     */
    uint32_t getHeight() const { return state_.height; }

    /**
     * @brief Get current difficulty
     *
     * @return Compact difficulty representation
     */
    uint32_t getCurrentDifficulty() const { return state_.difficulty; }

    /**
     * @brief Check if a block exists in the blockchain
     *
     * @param hash Block hash
     * @return true if block exists
     */
    bool hasBlock(const crypto::Hash256& hash) const;

    /**
     * @brief Validate a block (without adding it)
     *
     * @param block Block to validate
     * @return true if valid
     */
    bool validateBlock(const Block& block) const;

    /**
     * @brief Get block index by hash
     *
     * @param hash Block hash
     * @return Block index if found
     */
    BlockIndex* getBlockIndex(const crypto::Hash256& hash) const;

    /**
     * @brief Get the main chain from genesis to tip
     *
     * @return Vector of block hashes
     */
    std::vector<crypto::Hash256> getMainChain() const;

    /**
     * @brief Find the common ancestor of two blocks
     *
     * @param hash1 First block hash
     * @param hash2 Second block hash
     * @return Common ancestor hash
     */
    std::optional<crypto::Hash256> findCommonAncestor(
        const crypto::Hash256& hash1,
        const crypto::Hash256& hash2) const;

private:
    consensus::ChainParams params_;
    ChainState state_;

    // Block index (hash -> BlockIndex)
    std::unordered_map<crypto::Hash256, std::unique_ptr<BlockIndex>> blockIndex_;

    // Height index (height -> block hash) for main chain
    std::map<uint32_t, crypto::Hash256> heightIndex_;

    // Storage (would be database in production)
    std::unordered_map<crypto::Hash256, Block> blocks_;

    /**
     * @brief Set a new chain tip
     *
     * @param newTip New tip block index
     */
    void setChainTip(BlockIndex* newTip);

    /**
     * @brief Reorganize chain to a new tip
     *
     * @param newTip New chain tip
     * @return true if reorganization succeeded
     */
    bool reorganize(BlockIndex* newTip);

    /**
     * @brief Connect a block to the active chain
     *
     * Updates UTXO set and other state.
     *
     * @param block Block to connect
     * @return true if successful
     */
    bool connectBlock(const Block& block);

    /**
     * @brief Disconnect a block from the active chain
     *
     * Reverts UTXO changes.
     *
     * @param block Block to disconnect
     * @return true if successful
     */
    bool disconnectBlock(const Block& block);

    /**
     * @brief Calculate the next difficulty target
     *
     * @param prevIndex Previous block index
     * @return New difficulty target
     */
    uint32_t calculateNextDifficulty(const BlockIndex* prevIndex) const;

    /**
     * @brief Validate block headers
     *
     * @param block Block to validate
     * @param prevIndex Previous block index
     * @return true if headers are valid
     */
    bool validateBlockHeaders(const Block& block, const BlockIndex* prevIndex) const;

    /**
     * @brief Validate block transactions
     *
     * @param block Block to validate
     * @return true if transactions are valid
     */
    bool validateBlockTransactions(const Block& block) const;

    /**
     * @brief Check if a reorganization is needed
     *
     * @param newIndex New block index
     * @return true if reorganization should happen
     */
    bool shouldReorganize(const BlockIndex* newIndex) const;
};

}  // namespace core
}  // namespace ubuntu
