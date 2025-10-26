#include "ubuntu/monetary/peg_controller.h"
#include "ubuntu/rpc/rpc_server.h"
#include "ubuntu/core/logger.h"

#include <nlohmann/json.hpp>
#include <memory>

namespace ubuntu {
namespace monetary {

using json = nlohmann::json;

/**
 * @brief RPC interface for peg mechanism
 *
 * Provides two RPC methods:
 * - peg_getstatus: Get current peg state and health
 * - peg_gethistory: Get recent epoch events
 */
class PegRpcHandler {
public:
    explicit PegRpcHandler(std::shared_ptr<PegController> controller)
        : controller_(std::move(controller))
    {
        if (!controller_) {
            throw std::runtime_error("PegRpcHandler: controller cannot be null");
        }
    }

    /**
     * @brief Register RPC methods with server
     */
    void register_methods(rpc::RpcServer& server) {
        server.register_method("peg_getstatus", [this](const json& params) {
            return this->handle_get_status(params);
        });

        server.register_method("peg_gethistory", [this](const json& params) {
            return this->handle_get_history(params);
        });

        LOG_INFO("Peg RPC methods registered");
    }

private:
    /**
     * @brief RPC: peg_getstatus
     *
     * Returns current peg state, configuration, and health status.
     *
     * Request: {}
     *
     * Response:
     * {
     *   "enabled": true,
     *   "healthy": true,
     *   "epoch_id": 1234,
     *   "timestamp": 1234567890,
     *   "block_height": 100000,
     *   "price_usd": 1.05,
     *   "supply": "1000000000000000",
     *   "last_delta": "50000000000",
     *   "last_action": "expand",
     *   "last_reason": "Minting...",
     *   "circuit_breaker": false,
     *   "total_bond_debt": "0",
     *   "config": {
     *     "k": 0.05,
     *     "deadband": 0.01,
     *     "max_expansion": 0.05,
     *     "max_contraction": 0.05,
     *     "epoch_seconds": 3600
     *   }
     * }
     */
    json handle_get_status(const json& params) {
        try {
            PegState state = controller_->get_state();
            PegConfig config = controller_->get_config();
            bool healthy = controller_->is_healthy();

            json response;

            // Health and enablement
            response["enabled"] = config.enabled;
            response["healthy"] = healthy;
            response["circuit_breaker"] = state.circuit_breaker_triggered;

            // Current state
            response["epoch_id"] = state.epoch_id;
            response["timestamp"] = state.timestamp;
            response["block_height"] = state.block_height;

            // Price and supply (convert to human-readable)
            response["price_usd"] = static_cast<double>(state.last_price_scaled) /
                                    PegConstants::PRICE_SCALE;
            response["supply"] = int128_to_string(state.last_supply);
            response["last_delta"] = int128_to_string(state.last_delta);

            // Action diagnostics
            response["last_action"] = state.last_action;
            response["last_reason"] = state.last_reason;

            // Bond debt
            response["total_bond_debt"] = int128_to_string(state.total_bond_debt);
            response["bonds_issued_this_epoch"] = int128_to_string(state.bonds_issued_this_epoch);

            // Configuration (converted to human-readable percentages)
            json config_json;
            config_json["k"] = static_cast<double>(config.k_ppm) / PegConstants::PPM_SCALE;
            config_json["deadband"] = static_cast<double>(config.deadband_ppm) / PegConstants::PPM_SCALE;
            config_json["max_expansion"] = static_cast<double>(config.max_expansion_ppm) / PegConstants::PPM_SCALE;
            config_json["max_contraction"] = static_cast<double>(config.max_contraction_ppm) / PegConstants::PPM_SCALE;
            config_json["epoch_seconds"] = config.epoch_seconds;
            config_json["oracle_max_age_seconds"] = config.oracle_max_age_seconds;
            config_json["treasury_address"] = config.treasury_address;

            // PID parameters (if enabled)
            if (config.ki_ppm > 0 || config.kd_ppm > 0) {
                config_json["ki"] = static_cast<double>(config.ki_ppm) / PegConstants::PPM_SCALE;
                config_json["kd"] = static_cast<double>(config.kd_ppm) / PegConstants::PPM_SCALE;
                config_json["integral"] = int128_to_string(state.integral);
            }

            response["config"] = config_json;

            return response;

        } catch (const std::exception& e) {
            return create_error_response("Failed to get peg status: " + std::string(e.what()));
        }
    }

    /**
     * @brief RPC: peg_gethistory
     *
     * Returns recent epoch events from audit trail.
     *
     * Request:
     * {
     *   "count": 100  // Optional, default 100
     * }
     *
     * Response:
     * {
     *   "events": [
     *     {
     *       "epoch_id": 1234,
     *       "timestamp": 1234567890,
     *       "block_height": 100000,
     *       "price_usd": 1.05,
     *       "supply": "1000000000000000",
     *       "delta": "50000000000",
     *       "action": "expand",
     *       "reason": "Minting..."
     *     },
     *     ...
     *   ]
     * }
     */
    json handle_get_history(const json& params) {
        try {
            // Parse count parameter
            size_t count = 100;  // Default
            if (params.contains("count") && params["count"].is_number()) {
                count = params["count"].get<size_t>();
                count = std::min(count, size_t(1000));  // Cap at 1000
            }

            // Get events from controller
            std::vector<PegEvent> events = controller_->get_recent_events(count);

            // Convert to JSON
            json response;
            json events_json = json::array();

            for (const auto& event : events) {
                json event_json;
                event_json["epoch_id"] = event.epoch_id;
                event_json["timestamp"] = event.timestamp;
                event_json["block_height"] = event.block_height;
                event_json["price_usd"] = static_cast<double>(event.price_scaled) /
                                          PegConstants::PRICE_SCALE;
                event_json["supply"] = int128_to_string(event.supply);
                event_json["delta"] = int128_to_string(event.delta);
                event_json["action"] = event.action;
                event_json["reason"] = event.reason;

                events_json.push_back(event_json);
            }

            response["events"] = events_json;
            response["count"] = events.size();

            return response;

        } catch (const std::exception& e) {
            return create_error_response("Failed to get peg history: " + std::string(e.what()));
        }
    }

    // Helper functions

    std::string int128_to_string(const int128_t& value) const {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    json create_error_response(const std::string& error) const {
        json response;
        response["error"] = error;
        return response;
    }

    std::shared_ptr<PegController> controller_;
};

// ============================================================================
// Public API for integration
// ============================================================================

/**
 * @brief Register peg RPC methods with server
 *
 * Call this from daemon initialization to enable peg RPC endpoints.
 *
 * @param server RPC server instance
 * @param controller Peg controller instance
 *
 * @example
 * auto controller = std::make_shared<PegController>(...);
 * register_peg_rpc_methods(rpc_server, controller);
 */
void register_peg_rpc_methods(
    rpc::RpcServer& server,
    std::shared_ptr<PegController> controller)
{
    auto handler = std::make_shared<PegRpcHandler>(controller);
    handler->register_methods(server);
}

}  // namespace monetary
}  // namespace ubuntu
