#pragma once

#include "ubuntu/crypto/hash.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Forward declare RocksDB types to avoid including in header
namespace rocksdb {
class DB;
class ColumnFamilyHandle;
class WriteBatch;
struct Options;
}  // namespace rocksdb

namespace ubuntu {
namespace storage {

/**
 * @brief Database column families for organizing data
 */
enum class ColumnFamily {
    BLOCKS,        // Block data (hash -> serialized block)
    BLOCK_INDEX,   // Height index (height -> hash)
    TRANSACTIONS,  // Transaction index (txid -> block_hash + position)
    UTXO,          // UTXO set (outpoint -> utxo data)
    CHAIN_STATE,   // Chain metadata (best block, height, etc.)
    PEERS,         // Known peers (address -> peer info)
};

/**
 * @brief RocksDB database wrapper
 *
 * Provides high-level interface to RocksDB with column families
 * for different data types.
 */
class Database {
public:
    /**
     * @brief Open or create a database
     *
     * @param path Path to database directory
     * @param createIfMissing Create database if it doesn't exist
     */
    explicit Database(const std::string& path, bool createIfMissing = true);
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Open the database
     *
     * @return true if successful
     */
    bool open();

    /**
     * @brief Close the database
     */
    void close();

    /**
     * @brief Check if database is open
     */
    bool isOpen() const;

    /**
     * @brief Put a key-value pair
     *
     * @param cf Column family
     * @param key Key
     * @param value Value
     * @return true if successful
     */
    bool put(ColumnFamily cf,
             std::span<const uint8_t> key,
             std::span<const uint8_t> value);

    /**
     * @brief Put a key-value pair (string overload)
     */
    bool put(ColumnFamily cf, const std::string& key, const std::string& value);

    /**
     * @brief Get a value by key
     *
     * @param cf Column family
     * @param key Key
     * @return Value if found
     */
    std::optional<std::vector<uint8_t>> get(ColumnFamily cf,
                                             std::span<const uint8_t> key) const;

    /**
     * @brief Get a value by key (string overload)
     */
    std::optional<std::string> get(ColumnFamily cf, const std::string& key) const;

    /**
     * @brief Delete a key
     *
     * @param cf Column family
     * @param key Key
     * @return true if successful
     */
    bool remove(ColumnFamily cf, std::span<const uint8_t> key);

    /**
     * @brief Check if a key exists
     *
     * @param cf Column family
     * @param key Key
     * @return true if key exists
     */
    bool exists(ColumnFamily cf, std::span<const uint8_t> key) const;

    /**
     * @brief Batch write operations
     */
    class WriteBatchWrapper {
    public:
        explicit WriteBatchWrapper(Database* db);
        ~WriteBatchWrapper();

        void put(ColumnFamily cf,
                 std::span<const uint8_t> key,
                 std::span<const uint8_t> value);

        void remove(ColumnFamily cf, std::span<const uint8_t> key);

        bool commit();

    private:
        Database* db_;
        std::unique_ptr<rocksdb::WriteBatch> batch_;
    };

    /**
     * @brief Create a write batch for atomic operations
     *
     * @return Write batch wrapper
     */
    WriteBatchWrapper createBatch();

    /**
     * @brief Compact the database
     *
     * @param cf Column family to compact (or all if not specified)
     */
    void compact(std::optional<ColumnFamily> cf = std::nullopt);

    /**
     * @brief Get database statistics
     *
     * @return Statistics string
     */
    std::string getStats() const;

    /**
     * @brief Get approximate size on disk
     *
     * @return Size in bytes
     */
    uint64_t getApproximateSize() const;

private:
    std::string path_;
    bool createIfMissing_;

    std::unique_ptr<rocksdb::DB> db_;
    std::vector<std::unique_ptr<rocksdb::ColumnFamilyHandle>> columnFamilies_;

    /**
     * @brief Get column family handle
     */
    rocksdb::ColumnFamilyHandle* getColumnFamily(ColumnFamily cf) const;

    /**
     * @brief Get column family name
     */
    static std::string getColumnFamilyName(ColumnFamily cf);

    /**
     * @brief Create default options for RocksDB
     */
    static rocksdb::Options getDefaultOptions();
};

}  // namespace storage
}  // namespace ubuntu
