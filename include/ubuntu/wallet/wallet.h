#pragma once

#include "ubuntu/core/transaction.h"
#include "ubuntu/crypto/keys.h"
#include "ubuntu/storage/utxo_db.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace wallet {

/**
 * @brief Wallet address type
 */
enum class AddressType {
    P2PKH,   // Pay-to-Public-Key-Hash (legacy)
    P2SH,    // Pay-to-Script-Hash
    BECH32   // Bech32 (native SegWit)
};

/**
 * @brief Wallet address information
 */
struct WalletAddress {
    std::string address;
    crypto::PublicKey publicKey;
    std::string label;
    AddressType type;
    uint32_t index;  // Derivation index
    bool isChange;   // Change address flag
};

/**
 * @brief Wallet transaction record
 */
struct WalletTransaction {
    core::Transaction tx;
    crypto::Hash256 txHash;
    int64_t amount;        // Net amount (positive = received, negative = sent)
    uint32_t blockHeight;  // 0 if unconfirmed
    uint64_t timestamp;
    std::string comment;
};

/**
 * @brief HD Wallet implementation
 *
 * Hierarchical Deterministic wallet following BIP-32/39/44.
 * Manages keys, addresses, and UTXOs for a single wallet.
 */
class Wallet {
public:
    /**
     * @brief Create new wallet from mnemonic
     *
     * @param mnemonic BIP-39 mnemonic phrase
     * @param passphrase Optional BIP-39 passphrase
     * @param accountIndex Account index (default: 0)
     * @return New wallet instance
     */
    static std::unique_ptr<Wallet> createFromMnemonic(const std::string& mnemonic,
                                                       const std::string& passphrase = "",
                                                       uint32_t accountIndex = 0);

    /**
     * @brief Create new wallet with generated mnemonic
     *
     * @param wordCount Number of mnemonic words (12, 15, 18, 21, 24)
     * @param accountIndex Account index (default: 0)
     * @return New wallet instance and mnemonic phrase
     */
    static std::pair<std::unique_ptr<Wallet>, std::string> createNew(
        uint32_t wordCount = 24,
        uint32_t accountIndex = 0);

    /**
     * @brief Load wallet from file
     *
     * @param filename Wallet file path
     * @param password Encryption password
     * @return Loaded wallet instance
     */
    static std::unique_ptr<Wallet> loadFromFile(const std::string& filename,
                                                  const std::string& password);

    ~Wallet();

    /**
     * @brief Save wallet to file
     *
     * @param filename Wallet file path
     * @param password Encryption password
     * @return true if saved successfully
     */
    bool saveToFile(const std::string& filename, const std::string& password);

    /**
     * @brief Generate new receiving address
     *
     * @param label Optional address label
     * @param type Address type (default: P2PKH)
     * @return New address
     */
    std::string getNewAddress(const std::string& label = "", AddressType type = AddressType::P2PKH);

    /**
     * @brief Get new change address
     *
     * @return Change address
     */
    std::string getNewChangeAddress();

    /**
     * @brief Get all addresses
     *
     * @param includeChange Include change addresses
     * @return Vector of wallet addresses
     */
    std::vector<WalletAddress> getAddresses(bool includeChange = false) const;

    /**
     * @brief Get private key for address
     *
     * @param address Address string
     * @return Private key, or nullopt if not found
     */
    std::optional<crypto::PrivateKey> getPrivateKey(const std::string& address) const;

    /**
     * @brief Get wallet balance
     *
     * @param minConfirmations Minimum confirmations (default: 1)
     * @return Balance in satoshis
     */
    uint64_t getBalance(uint32_t minConfirmations = 1) const;

    /**
     * @brief Get unconfirmed balance
     *
     * @return Unconfirmed balance in satoshis
     */
    uint64_t getUnconfirmedBalance() const;

    /**
     * @brief List wallet transactions
     *
     * @param count Maximum number of transactions
     * @param skip Number of transactions to skip
     * @return Vector of wallet transactions
     */
    std::vector<WalletTransaction> listTransactions(size_t count = 10, size_t skip = 0) const;

    /**
     * @brief Get wallet transaction by hash
     *
     * @param txHash Transaction hash
     * @return Wallet transaction, or nullopt if not found
     */
    std::optional<WalletTransaction> getTransaction(const crypto::Hash256& txHash) const;

    /**
     * @brief Create and sign transaction
     *
     * @param recipients Map of address to amount
     * @param feeRate Fee rate in satoshis per byte
     * @param subtractFee Subtract fee from recipients
     * @return Signed transaction
     */
    core::Transaction createTransaction(const std::map<std::string, uint64_t>& recipients,
                                         uint64_t feeRate,
                                         bool subtractFee = false);

    /**
     * @brief Sign a transaction
     *
     * @param tx Transaction to sign
     * @return true if all inputs signed successfully
     */
    bool signTransaction(core::Transaction& tx);

    /**
     * @brief Add transaction to wallet
     *
     * @param tx Transaction
     * @param blockHeight Block height (0 if unconfirmed)
     * @return true if added successfully
     */
    bool addTransaction(const core::Transaction& tx, uint32_t blockHeight = 0);

    /**
     * @brief Rescan blockchain for wallet transactions
     *
     * @param fromHeight Starting block height
     */
    void rescan(uint32_t fromHeight = 0);

    /**
     * @brief Set UTXO database
     *
     * @param utxoDb UTXO database instance
     */
    void setUtxoDatabase(std::shared_ptr<storage::UTXODatabase> utxoDb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Wallet(const crypto::ExtendedKey& masterKey, uint32_t accountIndex);

    /**
     * @brief Derive key at path
     *
     * @param isChange Change address flag
     * @param index Address index
     * @return Derived key pair
     */
    crypto::KeyPair deriveKey(bool isChange, uint32_t index) const;

    /**
     * @brief Create address from public key
     *
     * @param publicKey Public key
     * @param type Address type
     * @return Address string
     */
    std::string createAddress(const crypto::PublicKey& publicKey, AddressType type) const;

    /**
     * @brief Select UTXOs for transaction
     *
     * @param targetAmount Target amount
     * @param feeRate Fee rate
     * @return Selected UTXOs and change amount
     */
    std::pair<std::vector<core::UTXO>, uint64_t> selectCoins(uint64_t targetAmount,
                                                               uint64_t feeRate) const;

    /**
     * @brief Calculate transaction fee
     *
     * @param inputCount Number of inputs
     * @param outputCount Number of outputs
     * @param feeRate Fee rate in sat/byte
     * @return Estimated fee
     */
    uint64_t estimateTransactionFee(size_t inputCount, size_t outputCount,
                                     uint64_t feeRate) const;
};

}  // namespace wallet
}  // namespace ubuntu
