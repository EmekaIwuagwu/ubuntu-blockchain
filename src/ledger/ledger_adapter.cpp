#include "ubuntu/ledger/ledger_adapter.h"
#include "ubuntu/core/chain.h"
#include "ubuntu/core/transaction.h"
#include "ubuntu/storage/utxo_db.h"
#include "ubuntu/crypto/bech32.h"
#include "ubuntu/core/logger.h"

#include <stdexcept>

namespace ubuntu {
namespace ledger {

using namespace ubuntu::monetary;

LedgerAdapter::LedgerAdapter(
    std::shared_ptr<core::Blockchain> blockchain,
    std::shared_ptr<storage::UTXODatabase> utxo_db)
    : blockchain_(std::move(blockchain))
    , utxo_db_(std::move(utxo_db))
{
    if (!blockchain_) {
        throw std::runtime_error("LedgerAdapter: blockchain cannot be null");
    }
    if (!utxo_db_) {
        throw std::runtime_error("LedgerAdapter: UTXO database cannot be null");
    }
}

int128_t LedgerAdapter::get_total_supply() const {
    if (!utxo_db_) {
        throw std::runtime_error("UTXO database not initialized");
    }

    // Iterate through all UTXOs and sum their values
    int128_t total_supply = 0;

    try {
        // Get all UTXOs from database
        auto utxos = utxo_db_->get_all_utxos();

        for (const auto& utxo : utxos) {
            // Skip provably unspendable outputs (OP_RETURN - used for burning)
            if (utxo.is_unspendable()) {
                continue;
            }

            total_supply += utxo.amount;
        }

        LOG_DEBUG("Total supply calculated: {} units ({} UTXOs)",
                  total_supply, utxos.size());

        return total_supply;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to calculate total supply: {}", e.what());
        throw std::runtime_error("Failed to calculate total supply: " + std::string(e.what()));
    }
}

int128_t LedgerAdapter::get_treasury_balance(const std::string& treasury_address) const {
    if (treasury_address.empty()) {
        throw std::invalid_argument("Treasury address cannot be empty");
    }

    // Validate address format
    if (!crypto::Bech32::is_valid_address(treasury_address)) {
        throw std::invalid_argument("Invalid treasury address format: " + treasury_address);
    }

    try {
        // Decode address to get public key hash
        auto decoded = crypto::Bech32::decode(treasury_address);
        if (!decoded) {
            throw std::invalid_argument("Failed to decode treasury address");
        }

        // Get all UTXOs for this address
        auto utxos = utxo_db_->get_utxos_for_address(treasury_address);

        int128_t balance = 0;
        for (const auto& utxo : utxos) {
            if (!utxo.is_unspendable()) {
                balance += utxo.amount;
            }
        }

        LOG_DEBUG("Treasury {} balance: {} units ({} UTXOs)",
                  treasury_address, balance, utxos.size());

        return balance;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get treasury balance: {}", e.what());
        throw std::runtime_error("Failed to get treasury balance: " + std::string(e.what()));
    }
}

bool LedgerAdapter::mint_to_treasury(int128_t amount, const std::string& treasury_address) {
    if (amount <= 0) {
        throw std::invalid_argument("Mint amount must be positive");
    }

    if (!crypto::Bech32::is_valid_address(treasury_address)) {
        throw std::invalid_argument("Invalid treasury address");
    }

    try {
        // Create a special "peg expansion" transaction
        // This is similar to a coinbase transaction but specifically for peg adjustments

        core::Transaction tx;

        // No inputs (like coinbase)
        // The transaction will have a special marker to identify it as a peg expansion

        // Create output to treasury
        core::TxOutput output;
        output.amount = static_cast<int64_t>(amount);  // Convert int128 to int64 (assumes amount fits)
        output.script_pubkey = create_p2pkh_script(treasury_address);

        tx.outputs.push_back(output);

        // Add special marker in transaction version or locktime field
        // to identify this as a peg expansion transaction
        tx.version = 2;  // Version 2 = peg transaction
        tx.locktime = 0xFFFFFFFF;  // Special locktime marker

        // Calculate transaction hash
        tx.txid = tx.calculate_hash();

        // Add transaction to mempool for inclusion in next block
        // Note: This requires mempool integration and miner support
        bool success = blockchain_->add_peg_expansion_transaction(tx);

        if (success) {
            LOG_INFO("Minted {} units to treasury {} (txid: {})",
                     amount, treasury_address, tx.txid.to_hex());
        } else {
            LOG_ERROR("Failed to add mint transaction to blockchain");
        }

        return success;

    } catch (const std::exception& e) {
        LOG_ERROR("Mint operation failed: {}", e.what());
        return false;
    }
}

bool LedgerAdapter::burn_from_treasury(int128_t amount, const std::string& treasury_address) {
    if (amount <= 0) {
        throw std::invalid_argument("Burn amount must be positive");
    }

    if (!crypto::Bech32::is_valid_address(treasury_address)) {
        throw std::invalid_argument("Invalid treasury address");
    }

    // Check treasury has sufficient balance
    int128_t treasury_balance = get_treasury_balance(treasury_address);
    if (treasury_balance < amount) {
        std::ostringstream oss;
        oss << "Insufficient treasury balance: " << treasury_balance
            << " < " << amount;
        throw std::runtime_error(oss.str());
    }

    try {
        // Create burn transaction
        core::Transaction tx;

        // Select UTXOs from treasury to cover amount + fee
        const int64_t FEE = 1000;  // 1000 satoshi fee
        int128_t total_input = 0;
        std::vector<storage::UTXO> selected_utxos;

        auto utxos = utxo_db_->get_utxos_for_address(treasury_address);
        for (const auto& utxo : utxos) {
            if (utxo.is_unspendable()) {
                continue;
            }

            selected_utxos.push_back(utxo);
            total_input += utxo.amount;

            if (total_input >= amount + FEE) {
                break;
            }
        }

        if (total_input < amount + FEE) {
            LOG_ERROR("Failed to select sufficient UTXOs for burn");
            return false;
        }

        // Create inputs from selected UTXOs
        for (const auto& utxo : selected_utxos) {
            core::TxInput input;
            input.prev_txid = utxo.txid;
            input.prev_output_index = utxo.output_index;
            // Note: Signing requires wallet private key access
            // This is a placeholder - actual implementation needs wallet integration
            input.script_sig = create_placeholder_signature();
            tx.inputs.push_back(input);
        }

        // Output 1: Provably unspendable (OP_RETURN) for burn amount
        core::TxOutput burn_output;
        burn_output.amount = static_cast<int64_t>(amount);
        burn_output.script_pubkey = create_op_return_script("PEG_BURN");
        tx.outputs.push_back(burn_output);

        // Output 2: Change back to treasury (if any)
        int128_t change = total_input - amount - FEE;
        if (change > 0) {
            core::TxOutput change_output;
            change_output.amount = static_cast<int64_t>(change);
            change_output.script_pubkey = create_p2pkh_script(treasury_address);
            tx.outputs.push_back(change_output);
        }

        // Calculate transaction hash
        tx.txid = tx.calculate_hash();

        // Add transaction to mempool
        bool success = blockchain_->add_peg_contraction_transaction(tx);

        if (success) {
            LOG_INFO("Burned {} units from treasury {} (txid: {})",
                     amount, treasury_address, tx.txid.to_hex());
        } else {
            LOG_ERROR("Failed to add burn transaction to blockchain");
        }

        return success;

    } catch (const std::exception& e) {
        LOG_ERROR("Burn operation failed: {}", e.what());
        return false;
    }
}

bool LedgerAdapter::is_healthy() const {
    if (!blockchain_ || !utxo_db_) {
        return false;
    }

    try {
        // Try to get current block height as health check
        uint32_t height = blockchain_->get_height();
        if (height == 0) {
            return false;  // No blocks
        }

        // Try to access UTXO database
        auto utxos = utxo_db_->get_all_utxos();

        return true;

    } catch (...) {
        return false;
    }
}

}  // namespace ledger
}  // namespace ubuntu
