#include "ubuntu/monetary/oracle_interface.h"
#include "ubuntu/core/logger.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>

namespace ubuntu {
namespace monetary {

/**
 * @brief Stub oracle implementation for testing and development
 *
 * Supports three modes:
 * 1. Fixed price (default: $1.00)
 * 2. File-based price feed (reads from file)
 * 3. Simulated random walk (for stress testing)
 */
class OracleStub : public IOracle {
public:
    /**
     * @brief Construct oracle stub
     *
     * Config format:
     * - "fixed:1.05" - Fixed price at $1.05
     * - "file:/path/to/price.txt" - Read price from file
     * - "random:1.00:0.05" - Random walk around $1.00 with ±5% variance
     */
    explicit OracleStub(const std::string& config)
        : mode_(Mode::Fixed)
        , fixed_price_scaled_(PegConstants::TARGET_PRICE)
        , file_path_()
        , random_center_(PegConstants::TARGET_PRICE)
        , random_variance_ppm_(50'000)  // ±5% default
    {
        parse_config(config);
        LOG_INFO("OracleStub initialized: mode={}, config='{}'",
                 mode_to_string(mode_), config);
    }

    std::optional<OraclePrice> get_latest_price() override {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            int64_t price_scaled;

            switch (mode_) {
                case Mode::Fixed:
                    price_scaled = fixed_price_scaled_;
                    break;

                case Mode::File:
                    price_scaled = read_price_from_file();
                    break;

                case Mode::Random:
                    price_scaled = generate_random_price();
                    break;

                default:
                    LOG_ERROR("Unknown oracle mode");
                    return std::nullopt;
            }

            // Create OraclePrice
            OraclePrice oracle_price;
            oracle_price.price_scaled = price_scaled;
            oracle_price.timestamp = get_current_timestamp();
            oracle_price.source = "oracle_stub";
            oracle_price.signature = {};  // No signature for stub

            LOG_DEBUG("OracleStub price: {:.6f} USD",
                     static_cast<double>(price_scaled) / PegConstants::PRICE_SCALE);

            return oracle_price;

        } catch (const std::exception& e) {
            LOG_ERROR("OracleStub failed to get price: {}", e.what());
            return std::nullopt;
        }
    }

    std::optional<OraclePrice> get_median_price(size_t count) override {
        // For stub, just return latest price
        return get_latest_price();
    }

    std::vector<OraclePrice> get_recent_prices(size_t count) override {
        // For stub, return single latest price
        std::vector<OraclePrice> prices;
        auto latest = get_latest_price();
        if (latest) {
            prices.push_back(*latest);
        }
        return prices;
    }

    /**
     * @brief Set fixed price (for testing)
     */
    void set_fixed_price(double price_usd) {
        std::lock_guard<std::mutex> lock(mutex_);
        fixed_price_scaled_ = static_cast<int64_t>(price_usd * PegConstants::PRICE_SCALE);
        mode_ = Mode::Fixed;
        LOG_INFO("OracleStub fixed price set to {:.6f} USD", price_usd);
    }

private:
    enum class Mode {
        Fixed,   // Fixed price
        File,    // Read from file
        Random   // Random walk simulation
    };

    void parse_config(const std::string& config) {
        if (config.empty() || config == "stub") {
            // Default: fixed at $1.00
            mode_ = Mode::Fixed;
            fixed_price_scaled_ = PegConstants::TARGET_PRICE;
            return;
        }

        // Parse "type:params" format
        size_t colon_pos = config.find(':');
        if (colon_pos == std::string::npos) {
            throw std::invalid_argument("Invalid oracle config format");
        }

        std::string type = config.substr(0, colon_pos);
        std::string params = config.substr(colon_pos + 1);

        if (type == "fixed") {
            mode_ = Mode::Fixed;
            double price = std::stod(params);
            fixed_price_scaled_ = static_cast<int64_t>(price * PegConstants::PRICE_SCALE);

        } else if (type == "file") {
            mode_ = Mode::File;
            file_path_ = params;

        } else if (type == "random") {
            mode_ = Mode::Random;

            // Parse "center:variance" (e.g., "1.00:0.05" for ±5%)
            size_t colon2 = params.find(':');
            if (colon2 != std::string::npos) {
                double center = std::stod(params.substr(0, colon2));
                double variance = std::stod(params.substr(colon2 + 1));

                random_center_ = static_cast<int64_t>(center * PegConstants::PRICE_SCALE);
                random_variance_ppm_ = static_cast<int64_t>(variance * PegConstants::PPM_SCALE);
            } else {
                double center = std::stod(params);
                random_center_ = static_cast<int64_t>(center * PegConstants::PRICE_SCALE);
            }

            // Initialize random number generator
            random_engine_.seed(std::random_device{}());

        } else {
            throw std::invalid_argument("Unknown oracle type: " + type);
        }
    }

    int64_t read_price_from_file() {
        std::ifstream file(file_path_);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open price file: " + file_path_);
        }

        std::string line;
        if (!std::getline(file, line)) {
            throw std::runtime_error("Failed to read price from file");
        }

        // Parse price (expect format: "1.05" for $1.05)
        double price = std::stod(line);
        return static_cast<int64_t>(price * PegConstants::PRICE_SCALE);
    }

    int64_t generate_random_price() {
        // Generate random price using normal distribution
        // around center with specified variance

        std::normal_distribution<double> distribution(0.0, 1.0);
        double random_factor = distribution(random_engine_);

        // Apply variance: price = center × (1 + random × variance)
        double variance_fraction = static_cast<double>(random_variance_ppm_) / PegConstants::PPM_SCALE;
        double price = static_cast<double>(random_center_) * (1.0 + random_factor * variance_fraction);

        int64_t price_scaled = static_cast<int64_t>(price);

        // Clamp to reasonable bounds (prevent negative or extreme prices)
        const int64_t MIN_PRICE = PegConstants::PRICE_SCALE / 10;  // $0.10
        const int64_t MAX_PRICE = PegConstants::PRICE_SCALE * 10;  // $10.00
        price_scaled = std::clamp(price_scaled, MIN_PRICE, MAX_PRICE);

        return price_scaled;
    }

    uint64_t get_current_timestamp() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count();
    }

    std::string mode_to_string(Mode mode) const {
        switch (mode) {
            case Mode::Fixed: return "fixed";
            case Mode::File: return "file";
            case Mode::Random: return "random";
            default: return "unknown";
        }
    }

    Mode mode_;
    int64_t fixed_price_scaled_;
    std::string file_path_;
    int64_t random_center_;
    int64_t random_variance_ppm_;
    std::mt19937_64 random_engine_;
    mutable std::mutex mutex_;
};

// ============================================================================
// OracleFactory Implementation
// ============================================================================

std::unique_ptr<IOracle> OracleFactory::create(
    const std::string& oracle_type,
    const std::string& config)
{
    if (oracle_type == "stub" || oracle_type.empty()) {
        return std::make_unique<OracleStub>(config);
    }

    // Future: Add other oracle types
    // - "chainlink": Chainlink price feed integration
    // - "uniswap": Uniswap TWAP oracle
    // - "aggregated": Multi-oracle aggregator with median

    throw std::invalid_argument("Unsupported oracle type: " + oracle_type);
}

}  // namespace monetary
}  // namespace ubuntu
