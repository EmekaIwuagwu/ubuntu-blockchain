#include "ubuntu/mempool/mempool.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace ubuntu {
namespace mempool {

// ============================================================================
// MempoolEntry Implementation
// ============================================================================

MempoolEntry::MempoolEntry(const core::Transaction& transaction)
    : tx(transaction),
      txHash(transaction.getHash()),
      fee(0),
      feeRate(0),
      size(transaction.getSize()),
      entryTime(std::chrono::system_clock::now()),
      height(0),
      signalsRBF(false) {

    // Check if transaction signals RBF (any input has sequence < MAX - 1)
    for (const auto& input : tx.inputs) {
        if (input.sequence < 0xFFFFFFFE) {
            signalsRBF = true;
            break;
        }
    }
}

// ============================================================================
// Mempool Implementation
// ============================================================================

Mempool::Mempool()
    : maxSize_(DEFAULT_MAX_SIZE), totalSize_(0), currentHeight_(0) {}

Mempool::~Mempool() = default;

bool Mempool::addTransaction(const core::Transaction& tx, uint64_t fee) {
    auto txHash = tx.getHash();

    // Check if already in mempool
    if (hasTransaction(txHash)) {
        spdlog::debug("Transaction {} already in mempool", txHash.toHex());
        return false;
    }

    // Create mempool entry
    MempoolEntry entry(tx);
    entry.fee = fee;
    entry.feeRate = calculateFeeRate(fee, entry.size);
    entry.height = currentHeight_;

    // Validate transaction
    if (!validateTransaction(entry)) {
        spdlog::warn("Transaction {} failed validation", txHash.toHex());
        return false;
    }

    // Check for conflicts
    auto conflict = checkConflicts(tx);
    if (conflict) {
        spdlog::info("Transaction {} conflicts with {}", txHash.toHex(), conflict->toHex());

        // Check if this is a valid RBF replacement
        auto it = transactions_.find(*conflict);
        if (it != transactions_.end() && it->second.signalsRBF) {
            // Allow replacement if fee is higher
            if (entry.feeRate <= it->second.feeRate) {
                spdlog::info("RBF rejected: fee not higher");
                return false;
            }

            spdlog::info("Replacing transaction via RBF");
            removeTransaction(*conflict);
        } else {
            return false;  // Conflict without RBF
        }
    }

    // Check mempool size limit
    if (totalSize_ + entry.size > maxSize_) {
        spdlog::info("Mempool full, evicting lowest priority transactions");
        evict(maxSize_ - entry.size);
    }

    // Update dependencies
    updateDependencies(entry);

    // Add to mempool
    transactions_[txHash] = entry;
    priorityQueue_.insert({entry.feeRate, txHash});
    totalSize_ += entry.size;

    // Track spent outputs
    for (const auto& input : tx.inputs) {
        if (!input.isCoinbase()) {
            spentOutputs_[input.previousOutput] = txHash;
        }
    }

    spdlog::info("Added transaction {} to mempool (fee: {} sat, rate: {} sat/byte)",
                 txHash.toHex(), fee, entry.feeRate);

    return true;
}

void Mempool::removeTransaction(const crypto::Hash256& txHash) {
    auto it = transactions_.find(txHash);
    if (it == transactions_.end()) {
        return;
    }

    const auto& entry = it->second;

    // Remove from priority queue
    auto range = priorityQueue_.equal_range(entry.feeRate);
    for (auto pqIt = range.first; pqIt != range.second; ++pqIt) {
        if (pqIt->second == txHash) {
            priorityQueue_.erase(pqIt);
            break;
        }
    }

    // Remove spent output tracking
    for (const auto& input : entry.tx.inputs) {
        if (!input.isCoinbase()) {
            spentOutputs_.erase(input.previousOutput);
        }
    }

    // Update size
    totalSize_ -= entry.size;

    // Remove transaction
    transactions_.erase(it);

    spdlog::debug("Removed transaction {} from mempool", txHash.toHex());
}

bool Mempool::hasTransaction(const crypto::Hash256& txHash) const {
    return transactions_.find(txHash) != transactions_.end();
}

std::optional<core::Transaction> Mempool::getTransaction(
    const crypto::Hash256& txHash) const {

    auto it = transactions_.find(txHash);
    if (it != transactions_.end()) {
        return it->second.tx;
    }
    return std::nullopt;
}

std::vector<core::Transaction> Mempool::getTopPriorityTransactions(
    size_t maxSize) const {

    std::vector<core::Transaction> result;
    size_t totalSize = 0;

    // Iterate from highest to lowest fee rate
    for (auto it = priorityQueue_.begin(); it != priorityQueue_.end(); ++it) {
        auto txIt = transactions_.find(it->second);
        if (txIt == transactions_.end()) {
            continue;
        }

        const auto& entry = txIt->second;

        if (totalSize + entry.size > maxSize) {
            break;  // Would exceed size limit
        }

        result.push_back(entry.tx);
        totalSize += entry.size;
    }

    return result;
}

std::vector<core::Transaction> Mempool::selectTransactionsForMining(
    size_t maxSize, size_t maxWeight) const {

    std::vector<core::Transaction> selected;
    std::set<crypto::Hash256> included;
    size_t totalSize = 0;

    // Greedy selection by fee rate
    // In a full implementation, this would use a more sophisticated algorithm
    // considering dependencies and package selection

    for (auto it = priorityQueue_.begin(); it != priorityQueue_.end(); ++it) {
        auto txIt = transactions_.find(it->second);
        if (txIt == transactions_.end()) {
            continue;
        }

        const auto& entry = txIt->second;

        // Check size limit
        if (totalSize + entry.size > maxSize) {
            continue;
        }

        // Check dependencies (must include all dependencies first)
        bool canInclude = true;
        for (const auto& dep : entry.depends) {
            if (included.find(dep) == included.end()) {
                canInclude = false;
                break;
            }
        }

        if (!canInclude) {
            continue;
        }

        // Add transaction
        selected.push_back(entry.tx);
        included.insert(entry.txHash);
        totalSize += entry.size;
    }

    spdlog::info("Selected {} transactions for mining ({} bytes, {} fees)",
                 selected.size(), totalSize, getTotalFees());

    return selected;
}

void Mempool::removeConflictingTransactions(const core::Block& block) {
    std::vector<crypto::Hash256> toRemove;

    // Remove all transactions in the block
    for (const auto& tx : block.transactions) {
        auto txHash = tx.getHash();
        if (hasTransaction(txHash)) {
            toRemove.push_back(txHash);
        }

        // Also remove transactions that spend the same outputs
        for (const auto& input : tx.inputs) {
            if (!input.isCoinbase()) {
                auto it = spentOutputs_.find(input.previousOutput);
                if (it != spentOutputs_.end()) {
                    toRemove.push_back(it->second);
                }
            }
        }
    }

    for (const auto& txHash : toRemove) {
        removeTransaction(txHash);
    }

    spdlog::info("Removed {} transactions after block confirmation", toRemove.size());
}

std::optional<crypto::Hash256> Mempool::checkConflicts(
    const core::Transaction& tx) const {

    for (const auto& input : tx.inputs) {
        if (!input.isCoinbase()) {
            auto it = spentOutputs_.find(input.previousOutput);
            if (it != spentOutputs_.end()) {
                return it->second;  // Conflict found
            }
        }
    }

    return std::nullopt;  // No conflicts
}

bool Mempool::replaceTransaction(const crypto::Hash256& oldTxHash,
                                  const core::Transaction& newTx,
                                  uint64_t newFee) {
    auto it = transactions_.find(oldTxHash);
    if (it == transactions_.end()) {
        return false;
    }

    const auto& oldEntry = it->second;

    // Check if old transaction signals RBF
    if (!oldEntry.signalsRBF) {
        spdlog::info("Cannot replace transaction {} - does not signal RBF",
                     oldTxHash.toHex());
        return false;
    }

    // Create new entry
    MempoolEntry newEntry(newTx);
    newEntry.fee = newFee;
    newEntry.feeRate = calculateFeeRate(newFee, newEntry.size);

    // Verify fee is higher
    if (newEntry.feeRate <= oldEntry.feeRate) {
        spdlog::info("RBF rejected: new fee rate ({}) not higher than old ({})",
                     newEntry.feeRate, oldEntry.feeRate);
        return false;
    }

    // Remove old transaction
    removeTransaction(oldTxHash);

    // Add new transaction
    return addTransaction(newTx, newFee);
}

uint64_t Mempool::getTotalFees() const {
    uint64_t total = 0;
    for (const auto& [hash, entry] : transactions_) {
        total += entry.fee;
    }
    return total;
}

void Mempool::evict(size_t targetSize) {
    // Evict lowest fee rate transactions until we reach target size
    while (totalSize_ > targetSize && !priorityQueue_.empty()) {
        // Get lowest fee rate transaction (at the end)
        auto it = std::prev(priorityQueue_.end());
        removeTransaction(it->second);
    }
}

Mempool::Stats Mempool::getStats() const {
    Stats stats;
    stats.count = transactions_.size();
    stats.totalSize = totalSize_;
    stats.totalFees = getTotalFees();

    if (transactions_.empty()) {
        stats.minFeeRate = 0;
        stats.maxFeeRate = 0;
        stats.medianFeeRate = 0;
        return stats;
    }

    // Min fee rate (last in priority queue)
    if (!priorityQueue_.empty()) {
        stats.minFeeRate = std::prev(priorityQueue_.end())->first;
        stats.maxFeeRate = priorityQueue_.begin()->first;
    }

    // Median fee rate
    std::vector<uint64_t> feeRates;
    feeRates.reserve(transactions_.size());
    for (const auto& [hash, entry] : transactions_) {
        feeRates.push_back(entry.feeRate);
    }
    std::sort(feeRates.begin(), feeRates.end());
    stats.medianFeeRate = feeRates[feeRates.size() / 2];

    return stats;
}

void Mempool::clear() {
    transactions_.clear();
    priorityQueue_.clear();
    spentOutputs_.clear();
    totalSize_ = 0;

    spdlog::info("Mempool cleared");
}

bool Mempool::validateTransaction(const MempoolEntry& entry) const {
    const auto& tx = entry.tx;

    // Basic validation
    if (tx.inputs.empty() || tx.outputs.empty()) {
        return false;
    }

    // Check size limits
    if (entry.size > MAX_TX_SIZE) {
        spdlog::info("Transaction too large: {} bytes", entry.size);
        return false;
    }

    // Check fee rate
    if (entry.feeRate < MIN_RELAY_FEE_RATE) {
        spdlog::info("Fee rate too low: {} sat/byte", entry.feeRate);
        return false;
    }

    // Check for dust outputs
    for (const auto& output : tx.outputs) {
        if (output.isDust()) {
            spdlog::info("Transaction has dust output");
            return false;
        }
    }

    // Check dependencies
    if (!checkDependencies(entry)) {
        return false;
    }

    return true;
}

bool Mempool::checkDependencies(const MempoolEntry& entry) const {
    // Check that we don't exceed ancestor/descendant limits
    // This is a simplified check - full implementation would track full dependency graph

    size_t ancestors = 0;
    for (const auto& dep : entry.depends) {
        if (hasTransaction(dep)) {
            ancestors++;
        }
    }

    if (ancestors > MAX_ANCESTORS) {
        spdlog::info("Too many ancestors: {}", ancestors);
        return false;
    }

    return true;
}

void Mempool::updateDependencies(const MempoolEntry& entry) {
    // Track dependencies (inputs that reference mempool transactions)
    for (const auto& input : entry.tx.inputs) {
        if (!input.isCoinbase()) {
            if (hasTransaction(input.previousOutput.txHash)) {
                // This transaction depends on another mempool transaction
                auto& mutableEntry = const_cast<MempoolEntry&>(entry);
                mutableEntry.depends.insert(input.previousOutput.txHash);

                // Update the parent transaction's spentBy set
                auto& parent = transactions_[input.previousOutput.txHash];
                parent.spentBy.insert(entry.txHash);
            }
        }
    }
}

uint64_t Mempool::calculateFeeRate(uint64_t fee, size_t size) const {
    if (size == 0) {
        return 0;
    }
    return fee / size;
}

}  // namespace mempool
}  // namespace ubuntu
