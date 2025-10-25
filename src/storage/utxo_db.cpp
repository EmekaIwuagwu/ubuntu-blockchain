#include "ubuntu/storage/utxo_db.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace ubuntu {
namespace storage {

// ============================================================================
// UTXODatabase Implementation
// ============================================================================

UTXODatabase::UTXODatabase(std::shared_ptr<Database> db) : db_(db) {}

UTXODatabase::~UTXODatabase() {
    flush();
}

bool UTXODatabase::load() {
    spdlog::info("Loading UTXO set from database...");

    // In a full implementation, we would iterate through the UTXO column family
    // and load all UTXOs into the cache. For now, we'll load on-demand.

    spdlog::info("UTXO database ready (on-demand loading)");
    return true;
}

bool UTXODatabase::addUTXO(const core::UTXO& utxo) {
    // Add to cache
    cache_[utxo.outpoint] = utxo;
    dirty_.insert(utxo.outpoint);

    // Evict cache if too large
    if (cache_.size() > MAX_CACHE_SIZE) {
        evictCache();
    }

    return true;
}

bool UTXODatabase::removeUTXO(const core::TxOutpoint& outpoint) {
    // Check if in cache
    auto it = cache_.find(outpoint);
    if (it != cache_.end()) {
        cache_.erase(it);
    }

    // Remove from database
    auto key = getKey(outpoint);
    if (!db_->remove(ColumnFamily::UTXO,
                     std::span<const uint8_t>(key.data(), key.size()))) {
        spdlog::warn("Failed to remove UTXO from database");
        return false;
    }

    dirty_.erase(outpoint);
    return true;
}

std::optional<core::UTXO> UTXODatabase::getUTXO(const core::TxOutpoint& outpoint) const {
    // Check cache first
    auto it = cache_.find(outpoint);
    if (it != cache_.end()) {
        return it->second;
    }

    // Load from database
    auto key = getKey(outpoint);
    auto data = db_->get(ColumnFamily::UTXO,
                         std::span<const uint8_t>(key.data(), key.size()));

    if (!data) {
        return std::nullopt;
    }

    return deserializeUTXO(std::span<const uint8_t>(data->data(), data->size()));
}

bool UTXODatabase::hasUTXO(const core::TxOutpoint& outpoint) const {
    // Check cache
    if (cache_.find(outpoint) != cache_.end()) {
        return true;
    }

    // Check database
    auto key = getKey(outpoint);
    return db_->exists(ColumnFamily::UTXO,
                       std::span<const uint8_t>(key.data(), key.size()));
}

std::vector<core::UTXO> UTXODatabase::getUTXOsForAddress(
    const std::vector<uint8_t>& scriptPubKey) const {

    std::vector<core::UTXO> utxos;

    // Search in cache
    for (const auto& [outpoint, utxo] : cache_) {
        if (utxo.output.scriptPubKey == scriptPubKey) {
            utxos.push_back(utxo);
        }
    }

    // In a full implementation, we would also query the database
    // using a secondary index on scriptPubKey

    return utxos;
}

uint64_t UTXODatabase::getBalance(const std::vector<uint8_t>& scriptPubKey) const {
    auto utxos = getUTXOsForAddress(scriptPubKey);

    uint64_t balance = 0;
    for (const auto& utxo : utxos) {
        balance += utxo.output.value;
    }

    return balance;
}

bool UTXODatabase::addUTXOs(const std::vector<core::UTXO>& utxos) {
    // Use batch for efficiency
    auto batch = db_->createBatch();

    for (const auto& utxo : utxos) {
        // Add to cache
        cache_[utxo.outpoint] = utxo;

        // Add to batch
        auto key = getKey(utxo.outpoint);
        auto value = serializeUTXO(utxo);

        batch.put(ColumnFamily::UTXO,
                  std::span<const uint8_t>(key.data(), key.size()),
                  std::span<const uint8_t>(value.data(), value.size()));
    }

    // Commit batch
    if (!batch.commit()) {
        spdlog::error("Failed to add UTXOs batch");
        return false;
    }

    // Clear dirty flags for these UTXOs
    for (const auto& utxo : utxos) {
        dirty_.erase(utxo.outpoint);
    }

    return true;
}

bool UTXODatabase::removeUTXOs(const std::vector<core::TxOutpoint>& outpoints) {
    // Use batch for efficiency
    auto batch = db_->createBatch();

    for (const auto& outpoint : outpoints) {
        // Remove from cache
        cache_.erase(outpoint);

        // Add to batch
        auto key = getKey(outpoint);
        batch.remove(ColumnFamily::UTXO,
                     std::span<const uint8_t>(key.data(), key.size()));
    }

    // Commit batch
    if (!batch.commit()) {
        spdlog::error("Failed to remove UTXOs batch");
        return false;
    }

    return true;
}

bool UTXODatabase::flush() {
    if (dirty_.empty()) {
        return true;
    }

    spdlog::info("Flushing {} dirty UTXOs to database", dirty_.size());

    auto batch = db_->createBatch();

    for (const auto& outpoint : dirty_) {
        auto it = cache_.find(outpoint);
        if (it == cache_.end()) {
            continue;  // Was removed
        }

        auto key = getKey(outpoint);
        auto value = serializeUTXO(it->second);

        batch.put(ColumnFamily::UTXO,
                  std::span<const uint8_t>(key.data(), key.size()),
                  std::span<const uint8_t>(value.data(), value.size()));
    }

    if (!batch.commit()) {
        spdlog::error("Failed to flush UTXOs");
        return false;
    }

    dirty_.clear();
    spdlog::info("UTXO flush complete");

    return true;
}

size_t UTXODatabase::size() const {
    // This is approximate - includes cache and database
    // In a full implementation, we'd track this more accurately
    return cache_.size();
}

uint64_t UTXODatabase::getTotalValue() const {
    uint64_t total = 0;

    for (const auto& [outpoint, utxo] : cache_) {
        total += utxo.output.value;
    }

    // In a full implementation, we'd also count database UTXOs not in cache

    return total;
}

bool UTXODatabase::clear() {
    cache_.clear();
    dirty_.clear();

    // In a full implementation, we'd also clear the database column family

    return true;
}

bool UTXODatabase::verify() const {
    // Verify that all cached UTXOs are valid
    for (const auto& [outpoint, utxo] : cache_) {
        // Check that outpoint matches
        if (outpoint != utxo.outpoint) {
            spdlog::error("UTXO verification failed: outpoint mismatch");
            return false;
        }

        // Check that value is positive
        if (utxo.output.value == 0) {
            spdlog::error("UTXO verification failed: zero value");
            return false;
        }
    }

    spdlog::info("UTXO set verified: {} entries", cache_.size());
    return true;
}

std::vector<uint8_t> UTXODatabase::serializeUTXO(const core::UTXO& utxo) const {
    // Use the UTXO's serialize method
    return utxo.serialize();
}

std::optional<core::UTXO> UTXODatabase::deserializeUTXO(
    std::span<const uint8_t> data) const {
    try {
        return core::UTXO::deserialize(data);
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize UTXO: {}", e.what());
        return std::nullopt;
    }
}

std::vector<uint8_t> UTXODatabase::getKey(const core::TxOutpoint& outpoint) const {
    // Key format: txhash (32 bytes) + vout (4 bytes)
    std::vector<uint8_t> key;
    key.reserve(36);

    // Add tx hash
    key.insert(key.end(), outpoint.txHash.begin(), outpoint.txHash.end());

    // Add vout (little-endian)
    key.push_back(outpoint.vout & 0xFF);
    key.push_back((outpoint.vout >> 8) & 0xFF);
    key.push_back((outpoint.vout >> 16) & 0xFF);
    key.push_back((outpoint.vout >> 24) & 0xFF);

    return key;
}

void UTXODatabase::evictCache() {
    // Simple eviction: flush dirty entries and remove oldest non-dirty entries
    spdlog::debug("Evicting UTXO cache entries");

    // Flush dirty entries first
    flush();

    // Remove entries until we're back under the limit
    size_t toRemove = cache_.size() - (MAX_CACHE_SIZE * 3 / 4);  // Remove 25%

    auto it = cache_.begin();
    while (toRemove > 0 && it != cache_.end()) {
        // Don't remove dirty entries
        if (dirty_.find(it->first) == dirty_.end()) {
            it = cache_.erase(it);
            --toRemove;
        } else {
            ++it;
        }
    }

    spdlog::debug("Cache eviction complete, new size: {}", cache_.size());
}

}  // namespace storage
}  // namespace ubuntu
