#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace monetary {

/**
 * @brief Price data from oracle source
 * 
 * Uses deterministic integer representation. Price is scaled by
 * PRICE_SCALE (1,000,000), so $1.00 = 1,000,000.
 */
struct OraclePrice {
    int64_t price_scaled;      // Price * PRICE_SCALE (e.g., $0.98 = 980,000)
    uint64_t timestamp;         // Unix epoch seconds
    std::string source;         // Source identifier or signer pubkey
    std::vector<uint8_t> signature; // Optional cryptographic signature
    
    /**
     * @brief Check if price is valid (non-negative, non-zero)
     */
    bool is_valid() const {
        return price_scaled > 0 && timestamp > 0;
    }
    
    /**
     * @brief Check if price is stale (older than max_age)
     */
    bool is_stale(uint64_t current_time, uint64_t max_age_seconds) const {
        return (current_time - timestamp) > max_age_seconds;
    }
};

/**
 * @brief Abstract oracle interface for price feeds
 * 
 * Implementations can be:
 * - File-based (for testing)
 * - RPC-based (calling external oracle service)
 * - On-chain (reading oracle transactions from blockchain)
 * - Aggregated (median of multiple sources)
 */
class IOracle {
public:
    virtual ~IOracle() = default;
    
    /**
     * @brief Get the latest price from oracle
     * 
     * @return Latest price if available, nullopt if unavailable
     */
    virtual std::optional<OraclePrice> get_latest_price() = 0;
    
    /**
     * @brief Get median of recent N prices (for aggregation)
     * 
     * @param count Number of recent prices to aggregate
     * @return Median price if available
     */
    virtual std::optional<OraclePrice> get_median_price(size_t count = 3) {
        // Default implementation: just return latest
        return get_latest_price();
    }
    
    /**
     * @brief Get all available prices (for diagnostics)
     */
    virtual std::vector<OraclePrice> get_recent_prices(size_t count = 10) {
        return {};
    }
};

/**
 * @brief Factory for creating oracle instances
 */
class OracleFactory {
public:
    /**
     * @brief Create oracle from configuration
     * 
     * @param oracle_type Type: "stub", "file", "rpc", "onchain", "aggregated"
     * @param config Configuration string (file path, RPC URL, etc.)
     * @return Oracle instance
     */
    static std::unique_ptr<IOracle> create(
        const std::string& oracle_type,
        const std::string& config);
};

} // namespace monetary
} // namespace ubuntu
