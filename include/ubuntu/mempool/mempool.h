#pragma once

#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace ubuntu {
namespace mempool {

/**
 * @brief Transaction entry in mempool
 */
struct MempoolEntry {
    core::Transaction tx;
    crypto::Hash256 txHash;
    uint64_t fee;                  // Transaction fee in satoshis
    uint64_t feeRate;              // Fee per byte (sat/byte)
    size_t size;                   // Transaction size in bytes
    std::chrono::system_clock::time_point entryTime;
    uint32_t height;               // Block height when added

    // RBF (Replace-By-Fee)
    bool signalsRBF;
    std::vector<crypto::Hash256> replacedBy;

    // Dependencies
    std::set<crypto::Hash256> depends;  // Transactions this depends on
    std::set<crypto::Hash256> spentBy;  // Transactions that spend this

    MempoolEntry() : fee(0), feeRate(0), size(0), height(0), signalsRBF(false) {}

    explicit MempoolEntry(const core::Transaction& transaction);
};

/**
 * @brief Transaction pool (mempool)
 *
 * Manages unconfirmed transactions with priority ordering for mining.
 */
class Mempool {
public:
    Mempool();
    ~Mempool();

    /**
     * @brief Add a transaction to the mempool
     *
     * Validates the transaction and adds it if valid.
     *
     * @param tx Transaction to add
     * @param fee Transaction fee (calculated externally)
     * @return true if added successfully
     */
    bool addTransaction(const core::Transaction& tx, uint64_t fee);

    /**
     * @brief Remove a transaction from the mempool
     *
     * @param txHash Transaction hash
     */
    void removeTransaction(const crypto::Hash256& txHash);

    /**
     * @brief Check if a transaction is in the mempool
     *
     * @param txHash Transaction hash
     * @return true if transaction exists
     */
    bool hasTransaction(const crypto::Hash256& txHash) const;

    /**
     * @brief Get a transaction from the mempool
     *
     * @param txHash Transaction hash
     * @return Transaction if found
     */
    std::optional<core::Transaction> getTransaction(const crypto::Hash256& txHash) const;

    /**
     * @brief Get top priority transactions for mining
     *
     * Returns transactions ordered by fee rate (highest first).
     *
     * @param maxSize Maximum total size in bytes
     * @return Vector of transactions
     */
    std::vector<core::Transaction> getTopPriorityTransactions(size_t maxSize) const;

    /**
     * @brief Get transactions for mining (optimized)
     *
     * Selects transactions to maximize fee while respecting dependencies.
     *
     * @param maxSize Maximum block size
     * @param maxWeight Maximum block weight
     * @return Vector of transactions
     */
    std::vector<core::Transaction> selectTransactionsForMining(
        size_t maxSize, size_t maxWeight) const;

    /**
     * @brief Remove transactions that conflict with a block
     *
     * Called when a block is added to remove confirmed transactions.
     *
     * @param block Block containing confirmed transactions
     */
    void removeConflictingTransactions(const core::Block& block);

    /**
     * @brief Check if a transaction conflicts with mempool
     *
     * @param tx Transaction to check
     * @return Conflicting transaction hash if found
     */
    std::optional<crypto::Hash256> checkConflicts(const core::Transaction& tx) const;

    /**
     * @brief Replace transaction (RBF - Replace-By-Fee)
     *
     * @param oldTx Transaction to replace
     * @param newTx New transaction
     * @param newFee Fee of new transaction
     * @return true if replacement succeeded
     */
    bool replaceTransaction(const crypto::Hash256& oldTxHash,
                            const core::Transaction& newTx,
                            uint64_t newFee);

    /**
     * @brief Get mempool size
     *
     * @return Number of transactions
     */
    size_t size() const { return transactions_.size(); }

    /**
     * @brief Get total mempool size in bytes
     *
     * @return Total size
     */
    size_t getTotalSize() const { return totalSize_; }

    /**
     * @brief Get total fees in mempool
     *
     * @return Total fees in satoshis
     */
    uint64_t getTotalFees() const;

    /**
     * @brief Evict lowest priority transactions if mempool is full
     *
     * @param targetSize Target size after eviction
     */
    void evict(size_t targetSize);

    /**
     * @brief Get mempool statistics
     */
    struct Stats {
        size_t count;
        size_t totalSize;
        uint64_t totalFees;
        uint64_t minFeeRate;
        uint64_t maxFeeRate;
        uint64_t medianFeeRate;
    };

    Stats getStats() const;

    /**
     * @brief Clear all transactions (for testing/rebuilding)
     */
    void clear();

    /**
     * @brief Set maximum mempool size
     *
     * @param maxSize Maximum size in bytes
     */
    void setMaxSize(size_t maxSize) { maxSize_ = maxSize; }

    size_t getMaxSize() const { return maxSize_; }

private:
    // Transaction storage
    std::unordered_map<crypto::Hash256, MempoolEntry> transactions_;

    // Priority queue (fee rate -> tx hash)
    std::multimap<uint64_t, crypto::Hash256, std::greater<>> priorityQueue_;

    // Spent outputs tracker (outpoint -> spending tx hash)
    std::unordered_map<core::TxOutpoint, crypto::Hash256> spentOutputs_;

    // Configuration
    size_t maxSize_;         // Maximum mempool size (default: 300 MB)
    size_t totalSize_;       // Current total size
    uint32_t currentHeight_; // Current blockchain height

    // Limits
    static constexpr size_t DEFAULT_MAX_SIZE = 300 * 1024 * 1024;  // 300 MB
    static constexpr size_t MAX_TX_SIZE = 100 * 1024;              // 100 KB
    static constexpr uint32_t MAX_ANCESTORS = 25;
    static constexpr uint32_t MAX_DESCENDANTS = 25;
    static constexpr uint64_t MIN_RELAY_FEE_RATE = 1;  // 1 sat/byte

    /**
     * @brief Validate a transaction for mempool entry
     */
    bool validateTransaction(const MempoolEntry& entry) const;

    /**
     * @brief Check transaction dependencies
     */
    bool checkDependencies(const MempoolEntry& entry) const;

    /**
     * @brief Update dependency tracking
     */
    void updateDependencies(const MempoolEntry& entry);

    /**
     * @brief Calculate transaction fee rate
     */
    uint64_t calculateFeeRate(uint64_t fee, size_t size) const;
};

}  // namespace mempool
}  // namespace ubuntu
