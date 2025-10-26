#pragma once

#include "ubuntu/monetary/peg_state.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ubuntu {

// Forward declarations
namespace core {
    class Blockchain;
}

namespace storage {
    class UTXODatabase;
}

namespace ledger {

/**
 * @brief Adapter interface for peg controller to interact with blockchain ledger
 *
 * This adapter provides an abstraction layer between the peg mechanism and the
 * actual UTXO blockchain implementation. It allows the peg controller to:
 * - Query total circulating supply
 * - Query treasury balance
 * - Mint new coins to treasury (expansion)
 * - Burn coins from treasury (contraction)
 *
 * The adapter ensures all operations are compatible with the existing UTXO
 * model without requiring changes to consensus rules or transaction formats.
 *
 * Implementation strategy:
 * - Minting: Create special coinbase-like transactions to treasury address
 * - Burning: Create provably unspendable outputs (OP_RETURN) from treasury
 * - Supply tracking: Sum all UTXO values minus provably unspendable outputs
 *
 * Thread safety: Implementations must be thread-safe.
 */
class LedgerAdapter {
public:
    /**
     * @brief Construct a ledger adapter
     *
     * @param blockchain Pointer to blockchain instance
     * @param utxo_db Pointer to UTXO database
     */
    LedgerAdapter(
        std::shared_ptr<core::Blockchain> blockchain,
        std::shared_ptr<storage::UTXODatabase> utxo_db);

    virtual ~LedgerAdapter() = default;

    /**
     * @brief Get total circulating supply
     *
     * Calculates the sum of all unspent transaction outputs (UTXOs)
     * minus any provably unspendable outputs.
     *
     * @return Total supply in smallest units (satoshi-like)
     *
     * @throws std::runtime_error if supply cannot be calculated
     */
    virtual monetary::int128_t get_total_supply() const;

    /**
     * @brief Get balance of treasury address
     *
     * Sums all UTXOs controlled by the specified treasury address.
     *
     * @param treasury_address Bech32 address of treasury
     * @return Treasury balance in smallest units
     *
     * @throws std::runtime_error if address is invalid
     */
    virtual monetary::int128_t get_treasury_balance(const std::string& treasury_address) const;

    /**
     * @brief Mint new coins to treasury (expansion)
     *
     * Creates a special transaction that mints new coins to the treasury address.
     * This is implemented as a coinbase-like output in a special "peg adjustment"
     * transaction that is validated by the peg mechanism.
     *
     * Implementation:
     * 1. Create transaction with no inputs (similar to coinbase)
     * 2. Single output to treasury_address with amount
     * 3. Special marker in scriptSig: "PEG_EXPANSION"
     * 4. Add transaction to mempool for inclusion in next block
     *
     * @param amount Amount to mint (in smallest units)
     * @param treasury_address Destination address for new coins
     *
     * @return true if minting successful, false otherwise
     *
     * @throws std::invalid_argument if amount <= 0 or address invalid
     */
    virtual bool mint_to_treasury(monetary::int128_t amount, const std::string& treasury_address);

    /**
     * @brief Burn coins from treasury (contraction)
     *
     * Creates a transaction that permanently destroys coins by sending them
     * to a provably unspendable output (OP_RETURN).
     *
     * Implementation:
     * 1. Select UTXOs from treasury_address totaling >= amount
     * 2. Create transaction with:
     *    - Inputs: Selected UTXOs
     *    - Outputs:
     *      a) OP_RETURN with amount (provably unspendable)
     *      b) Change output back to treasury (if any)
     * 3. Sign with treasury private key (requires wallet integration)
     * 4. Add transaction to mempool
     *
     * @param amount Amount to burn (in smallest units)
     * @param treasury_address Source address for coins to burn
     *
     * @return true if burning successful, false otherwise
     *
     * @throws std::invalid_argument if amount <= 0 or address invalid
     * @throws std::runtime_error if treasury has insufficient balance
     *
     * @note This requires the wallet to have the private key for treasury_address
     */
    virtual bool burn_from_treasury(monetary::int128_t amount, const std::string& treasury_address);

    /**
     * @brief Check if ledger adapter is operational
     *
     * @return true if blockchain and UTXO DB are accessible
     */
    virtual bool is_healthy() const;

protected:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<storage::UTXODatabase> utxo_db_;
};

}  // namespace ledger
}  // namespace ubuntu
