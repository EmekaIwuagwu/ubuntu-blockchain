#pragma once

#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/hash.h"
#include "ubuntu/storage/database.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ubuntu {
namespace storage {

/**
 * @brief UTXO database for managing unspent transaction outputs
 *
 * Provides efficient lookup, addition, and removal of UTXOs.
 * Uses an in-memory cache backed by RocksDB for persistence.
 */
class UTXODatabase {
public:
    /**
     * @brief Construct UTXO database
     *
     * @param db Underlying database
     */
    explicit UTXODatabase(std::shared_ptr<Database> db);
    ~UTXODatabase();

    /**
     * @brief Load UTXO set from database into memory
     *
     * @return true if successful
     */
    bool load();

    /**
     * @brief Add a UTXO
     *
     * @param utxo UTXO to add
     * @return true if successful
     */
    bool addUTXO(const core::UTXO& utxo);

    /**
     * @brief Remove a UTXO (mark as spent)
     *
     * @param outpoint Output point to remove
     * @return true if UTXO existed and was removed
     */
    bool removeUTXO(const core::TxOutpoint& outpoint);

    /**
     * @brief Get a UTXO
     *
     * @param outpoint Output point
     * @return UTXO if found
     */
    std::optional<core::UTXO> getUTXO(const core::TxOutpoint& outpoint) const;

    /**
     * @brief Check if a UTXO exists
     *
     * @param outpoint Output point
     * @return true if UTXO exists
     */
    bool hasUTXO(const core::TxOutpoint& outpoint) const;

    /**
     * @brief Get all UTXOs for an address
     *
     * @param scriptPubKey Script public key (address)
     * @return Vector of UTXOs
     */
    std::vector<core::UTXO> getUTXOsForAddress(
        const std::vector<uint8_t>& scriptPubKey) const;

    /**
     * @brief Get balance for an address
     *
     * @param scriptPubKey Script public key (address)
     * @return Total balance in satoshis
     */
    uint64_t getBalance(const std::vector<uint8_t>& scriptPubKey) const;

    /**
     * @brief Batch add UTXOs (for connecting a block)
     *
     * @param utxos Vector of UTXOs to add
     * @return true if successful
     */
    bool addUTXOs(const std::vector<core::UTXO>& utxos);

    /**
     * @brief Batch remove UTXOs (for disconnecting a block)
     *
     * @param outpoints Vector of outpoints to remove
     * @return true if successful
     */
    bool removeUTXOs(const std::vector<core::TxOutpoint>& outpoints);

    /**
     * @brief Flush in-memory cache to database
     *
     * @return true if successful
     */
    bool flush();

    /**
     * @brief Get UTXO set size (number of UTXOs)
     *
     * @return Number of UTXOs
     */
    size_t size() const;

    /**
     * @brief Get total value of all UTXOs
     *
     * @return Total value in satoshis
     */
    uint64_t getTotalValue() const;

    /**
     * @brief Clear all UTXOs (for testing/rebuilding)
     *
     * @return true if successful
     */
    bool clear();

    /**
     * @brief Verify UTXO set integrity
     *
     * @return true if all UTXOs are valid
     */
    bool verify() const;

private:
    std::shared_ptr<Database> db_;

    // In-memory cache for fast lookups
    std::unordered_map<core::TxOutpoint, core::UTXO> cache_;

    // Dirty set for tracking changes
    std::unordered_set<core::TxOutpoint> dirty_;

    // Cache size limit (number of entries)
    static constexpr size_t MAX_CACHE_SIZE = 100'000;

    /**
     * @brief Serialize UTXO to bytes
     */
    std::vector<uint8_t> serializeUTXO(const core::UTXO& utxo) const;

    /**
     * @brief Deserialize UTXO from bytes
     */
    std::optional<core::UTXO> deserializeUTXO(std::span<const uint8_t> data) const;

    /**
     * @brief Get UTXO key for database
     */
    std::vector<uint8_t> getKey(const core::TxOutpoint& outpoint) const;

    /**
     * @brief Evict old entries from cache if needed
     */
    void evictCache();
};

}  // namespace storage
}  // namespace ubuntu
