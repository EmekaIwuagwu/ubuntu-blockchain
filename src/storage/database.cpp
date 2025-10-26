#include "ubuntu/storage/database.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace ubuntu {
namespace storage {

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database(const std::string& path, bool createIfMissing)
    : path_(path), createIfMissing_(createIfMissing) {}

Database::~Database() {
    close();
}

bool Database::open() {
    if (isOpen()) {
        spdlog::warn("Database already open");
        return true;
    }

    rocksdb::Options options = getDefaultOptions();
    options.create_if_missing = createIfMissing_;

    // Define column families
    std::vector<rocksdb::ColumnFamilyDescriptor> columnFamilyDescriptors;
    columnFamilyDescriptors.push_back(rocksdb::ColumnFamilyDescriptor(
        rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));

    // Add custom column families
    for (int i = static_cast<int>(ColumnFamily::BLOCKS);
         i <= static_cast<int>(ColumnFamily::PEERS); ++i) {
        auto cf = static_cast<ColumnFamily>(i);
        columnFamilyDescriptors.push_back(rocksdb::ColumnFamilyDescriptor(
            getColumnFamilyName(cf), rocksdb::ColumnFamilyOptions()));
    }

    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    rocksdb::DB* dbPtr = nullptr;

    rocksdb::Status status = rocksdb::DB::Open(
        options, path_, columnFamilyDescriptors, &handles, &dbPtr);

    if (!status.ok()) {
        // Try creating the database with column families
        if (createIfMissing_) {
            spdlog::info("Creating new database at {}", path_);

            // First create database with default column family
            rocksdb::DB* tempDb;
            status = rocksdb::DB::Open(options, path_, &tempDb);

            if (!status.ok()) {
                spdlog::error("Failed to create database: {}", status.ToString());
                return false;
            }

            // Create column families
            for (int i = static_cast<int>(ColumnFamily::BLOCKS);
                 i <= static_cast<int>(ColumnFamily::PEERS); ++i) {
                auto cf = static_cast<ColumnFamily>(i);
                rocksdb::ColumnFamilyHandle* cfHandle;
                status = tempDb->CreateColumnFamily(
                    rocksdb::ColumnFamilyOptions(), getColumnFamilyName(cf), &cfHandle);

                if (!status.ok()) {
                    spdlog::error("Failed to create column family {}: {}",
                                  getColumnFamilyName(cf), status.ToString());
                    delete tempDb;
                    return false;
                }
                delete cfHandle;
            }

            delete tempDb;

            // Reopen with all column families
            status = rocksdb::DB::Open(
                options, path_, columnFamilyDescriptors, &handles, &dbPtr);
        }

        if (!status.ok()) {
            spdlog::error("Failed to open database: {}", status.ToString());
            return false;
        }
    }

    db_.reset(dbPtr);

    // Store column family handles
    columnFamilies_.clear();
    for (auto* handle : handles) {
        columnFamilies_.emplace_back(handle);
    }

    spdlog::info("Database opened successfully at {}", path_);
    return true;
}

void Database::close() {
    if (!isOpen()) {
        return;
    }

    columnFamilies_.clear();
    db_.reset();

    spdlog::info("Database closed");
}

bool Database::isOpen() const {
    return db_ != nullptr;
}

bool Database::put(ColumnFamily cf,
                   std::span<const uint8_t> key,
                   std::span<const uint8_t> value) {
    if (!isOpen()) {
        spdlog::error("Database not open");
        return false;
    }

    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());
    rocksdb::Slice valueSlice(reinterpret_cast<const char*>(value.data()), value.size());

    rocksdb::Status status = db_->Put(
        rocksdb::WriteOptions(), getColumnFamily(cf), keySlice, valueSlice);

    if (!status.ok()) {
        spdlog::error("Put failed: {}", status.ToString());
        return false;
    }

    return true;
}

bool Database::put(ColumnFamily cf, const std::string& key, const std::string& value) {
    return put(cf,
               std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(key.data()),
                                        key.size()),
               std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()),
                                        value.size()));
}

std::optional<std::vector<uint8_t>> Database::get(ColumnFamily cf,
                                                   std::span<const uint8_t> key) const {
    if (!isOpen()) {
        spdlog::error("Database not open");
        return std::nullopt;
    }

    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());
    std::string value;

    rocksdb::Status status = db_->Get(
        rocksdb::ReadOptions(), getColumnFamily(cf), keySlice, &value);

    if (status.ok()) {
        return std::vector<uint8_t>(value.begin(), value.end());
    } else if (status.IsNotFound()) {
        return std::nullopt;
    } else {
        spdlog::error("Get failed: {}", status.ToString());
        return std::nullopt;
    }
}

std::optional<std::string> Database::get(ColumnFamily cf, const std::string& key) const {
    auto result = get(cf, std::span<const uint8_t>(
                              reinterpret_cast<const uint8_t*>(key.data()), key.size()));

    if (result) {
        return std::string(result->begin(), result->end());
    }

    return std::nullopt;
}

bool Database::remove(ColumnFamily cf, std::span<const uint8_t> key) {
    if (!isOpen()) {
        spdlog::error("Database not open");
        return false;
    }

    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());

    rocksdb::Status status = db_->Delete(
        rocksdb::WriteOptions(), getColumnFamily(cf), keySlice);

    if (!status.ok()) {
        spdlog::error("Delete failed: {}", status.ToString());
        return false;
    }

    return true;
}

bool Database::exists(ColumnFamily cf, std::span<const uint8_t> key) const {
    if (!isOpen()) {
        return false;
    }

    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());
    std::string value;

    rocksdb::Status status = db_->Get(
        rocksdb::ReadOptions(), getColumnFamily(cf), keySlice, &value);

    return status.ok();
}

void Database::compact(std::optional<ColumnFamily> cf) {
    if (!isOpen()) {
        spdlog::error("Database not open");
        return;
    }

    if (cf) {
        spdlog::info("Compacting column family: {}", getColumnFamilyName(*cf));
        db_->CompactRange(rocksdb::CompactRangeOptions(), getColumnFamily(*cf),
                          nullptr, nullptr);
    } else {
        spdlog::info("Compacting entire database");
        for (int i = 0; i < static_cast<int>(columnFamilies_.size()); ++i) {
            db_->CompactRange(rocksdb::CompactRangeOptions(),
                              columnFamilies_[i].get(), nullptr, nullptr);
        }
    }
}

std::string Database::getStats() const {
    if (!isOpen()) {
        return "Database not open";
    }

    std::string stats;
    db_->GetProperty("rocksdb.stats", &stats);
    return stats;
}

uint64_t Database::getApproximateSize() const {
    if (!isOpen()) {
        return 0;
    }

    uint64_t totalSize = 0;

    for (const auto& cf : columnFamilies_) {
        uint64_t size = 0;
        db_->GetIntProperty(cf.get(), "rocksdb.total-sst-files-size", &size);
        totalSize += size;
    }

    return totalSize;
}

rocksdb::ColumnFamilyHandle* Database::getColumnFamily(ColumnFamily cf) const {
    // Default column family is at index 0
    // Custom column families start at index 1
    int index = static_cast<int>(cf) + 1;

    if (index < 0 || index >= static_cast<int>(columnFamilies_.size())) {
        throw std::out_of_range("Invalid column family");
    }

    return columnFamilies_[index].get();
}

std::string Database::getColumnFamilyName(ColumnFamily cf) {
    switch (cf) {
        case ColumnFamily::BLOCKS:
            return "blocks";
        case ColumnFamily::BLOCK_INDEX:
            return "block_index";
        case ColumnFamily::TRANSACTIONS:
            return "transactions";
        case ColumnFamily::UTXO:
            return "utxo";
        case ColumnFamily::CHAIN_STATE:
            return "chain_state";
        case ColumnFamily::PEERS:
            return "peers";
        default:
            return "unknown";
    }
}

rocksdb::Options Database::getDefaultOptions() {
    rocksdb::Options options;

    // Performance tuning
    options.max_open_files = 10000;
    options.write_buffer_size = 64 * 1024 * 1024;  // 64 MB
    options.max_write_buffer_number = 3;
    options.target_file_size_base = 64 * 1024 * 1024;

    // Compression (LZ4 is faster, Snappy is good too)
    options.compression = rocksdb::kLZ4Compression;

    // Bloom filters for faster reads
    rocksdb::BlockBasedTableOptions tableOptions;
    tableOptions.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(tableOptions));

    // Parallelism
    options.IncreaseParallelism(std::thread::hardware_concurrency());

    // Enable statistics
    options.statistics = rocksdb::CreateDBStatistics();

    return options;
}

// ============================================================================
// WriteBatchWrapper Implementation
// ============================================================================

Database::WriteBatchWrapper::WriteBatchWrapper(Database* db)
    : db_(db), batch_(std::make_unique<rocksdb::WriteBatch>()) {}

Database::WriteBatchWrapper::~WriteBatchWrapper() = default;

void Database::WriteBatchWrapper::put(ColumnFamily cf,
                                      std::span<const uint8_t> key,
                                      std::span<const uint8_t> value) {
    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());
    rocksdb::Slice valueSlice(reinterpret_cast<const char*>(value.data()), value.size());

    batch_->Put(db_->getColumnFamily(cf), keySlice, valueSlice);
}

void Database::WriteBatchWrapper::put(ColumnFamily cf,
                                      const std::string& key,
                                      const std::string& value) {
    put(cf,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()),
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()), value.size()));
}

void Database::WriteBatchWrapper::remove(ColumnFamily cf,
                                         std::span<const uint8_t> key) {
    rocksdb::Slice keySlice(reinterpret_cast<const char*>(key.data()), key.size());
    batch_->Delete(db_->getColumnFamily(cf), keySlice);
}

bool Database::WriteBatchWrapper::commit(bool sync) {
    if (!db_->isOpen()) {
        spdlog::error("Database not open - cannot commit batch");
        return false;
    }

    // Configure write options
    rocksdb::WriteOptions writeOptions;
    writeOptions.sync = sync;  // Force sync to disk if requested

    // Disable write-ahead log for performance if not syncing
    // (WAL still provides crash recovery even without sync)
    writeOptions.disableWAL = false;

    // Commit the batch atomically
    rocksdb::Status status = db_->db_->Write(writeOptions, batch_.get());

    if (!status.ok()) {
        spdlog::error("Atomic batch write failed: {}", status.ToString());
        return false;
    }

    if (sync) {
        spdlog::debug("Atomic batch committed with sync ({} operations)", count());
    }

    return true;
}

size_t Database::WriteBatchWrapper::count() const {
    return batch_->Count();
}

void Database::WriteBatchWrapper::clear() {
    batch_->Clear();
}

Database::WriteBatchWrapper Database::createBatch() {
    return WriteBatchWrapper(this);
}

}  // namespace storage
}  // namespace ubuntu
