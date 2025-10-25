#pragma once

#include "ubuntu/consensus/chainparams.h"
#include "ubuntu/core/block.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/crypto/keys.h"
#include "ubuntu/mempool/mempool.h"
#include "ubuntu/storage/utxo_db.h"

#include <memory>
#include <optional>
#include <vector>

namespace ubuntu {
namespace mining {

/**
 * @brief Block template for mining
 *
 * Contains all information needed to mine a block.
 */
struct BlockTemplate {
    core::Block block;              // Block to mine
    uint64_t totalFees;             // Total transaction fees
    std::vector<uint64_t> txFees;   // Individual transaction fees
    uint32_t sigOps;                // Total signature operations
    uint32_t weight;                // Block weight
};

/**
 * @brief Block assembler for mining
 *
 * Assembles block templates by selecting transactions from the mempool.
 * Optimizes for maximum fee collection while respecting block limits.
 */
class BlockAssembler {
public:
    /**
     * @brief Construct block assembler
     *
     * @param blockchain Blockchain instance
     * @param mempool Transaction mempool
     * @param utxoDb UTXO database
     * @param params Chain parameters
     */
    BlockAssembler(std::shared_ptr<core::Blockchain> blockchain,
                   std::shared_ptr<mempool::Mempool> mempool,
                   std::shared_ptr<storage::UTXODatabase> utxoDb,
                   const consensus::ChainParams& params);

    ~BlockAssembler();

    /**
     * @brief Create a new block template
     *
     * Selects transactions from mempool and creates a block ready for mining.
     *
     * @param minerAddress Address to receive coinbase reward
     * @return Block template, or nullopt if failed
     */
    std::optional<BlockTemplate> createBlockTemplate(
        const std::vector<uint8_t>& minerScriptPubKey);

    /**
     * @brief Update an existing block template
     *
     * Refreshes the block with new transactions from mempool.
     *
     * @param blockTemplate Template to update
     * @return true if updated successfully
     */
    bool updateBlockTemplate(BlockTemplate& blockTemplate);

    /**
     * @brief Set maximum block size
     *
     * @param maxSize Maximum size in bytes
     */
    void setMaxBlockSize(uint64_t maxSize);

    /**
     * @brief Set maximum block weight
     *
     * @param maxWeight Maximum weight
     */
    void setMaxBlockWeight(uint64_t maxWeight);

    /**
     * @brief Set minimum fee rate for inclusion
     *
     * @param minFeeRate Minimum fee rate in sat/byte
     */
    void setMinFeeRate(uint64_t minFeeRate);

    /**
     * @brief Get block assembly statistics
     */
    struct AssemblyStats {
        size_t txIncluded;      // Transactions included
        size_t txExcluded;      // Transactions excluded
        uint64_t totalFees;     // Total fees collected
        uint64_t blockSize;     // Block size in bytes
        uint64_t blockWeight;   // Block weight
        uint32_t sigOps;        // Total signature operations
    };

    AssemblyStats getLastAssemblyStats() const;

private:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<mempool::Mempool> mempool_;
    std::shared_ptr<storage::UTXODatabase> utxoDb_;
    consensus::ChainParams params_;

    // Configuration
    uint64_t maxBlockSize_;
    uint64_t maxBlockWeight_;
    uint64_t minFeeRate_;

    // Statistics
    AssemblyStats lastStats_;

    /**
     * @brief Create coinbase transaction
     *
     * @param height Block height
     * @param minerScriptPubKey Miner's script public key
     * @param totalFees Total fees from included transactions
     * @return Coinbase transaction
     */
    core::Transaction createCoinbaseTransaction(uint32_t height,
                                                 const std::vector<uint8_t>& minerScriptPubKey,
                                                 uint64_t totalFees);

    /**
     * @brief Calculate block reward for height
     *
     * @param height Block height
     * @return Block subsidy in satoshis
     */
    uint64_t calculateBlockReward(uint32_t height) const;

    /**
     * @brief Select transactions from mempool
     *
     * Uses a greedy algorithm to maximize fees while respecting limits.
     *
     * @param maxSize Maximum block size
     * @param maxWeight Maximum block weight
     * @return Selected transactions and total fees
     */
    std::pair<std::vector<core::Transaction>, uint64_t> selectTransactions(uint64_t maxSize,
                                                                             uint64_t maxWeight);

    /**
     * @brief Calculate transaction weight
     *
     * @param tx Transaction
     * @return Weight units
     */
    uint32_t calculateTxWeight(const core::Transaction& tx) const;

    /**
     * @brief Count signature operations in transaction
     *
     * @param tx Transaction
     * @return Number of signature operations
     */
    uint32_t countSigOps(const core::Transaction& tx) const;

    /**
     * @brief Validate transaction can be included
     *
     * @param tx Transaction to validate
     * @return true if valid for inclusion
     */
    bool canIncludeTransaction(const core::Transaction& tx) const;

    /**
     * @brief Get transaction fee
     *
     * @param tx Transaction
     * @return Fee in satoshis, or 0 if cannot calculate
     */
    uint64_t getTransactionFee(const core::Transaction& tx) const;
};

}  // namespace mining
}  // namespace ubuntu
