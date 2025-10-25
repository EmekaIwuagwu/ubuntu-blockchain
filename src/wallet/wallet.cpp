#include "ubuntu/wallet/wallet.h"

#include "ubuntu/crypto/base58.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

namespace ubuntu {
namespace wallet {

// ============================================================================
// Wallet::Impl
// ============================================================================

struct Wallet::Impl {
    crypto::ExtendedKey masterKey;
    uint32_t accountIndex;

    // Address tracking
    mutable std::mutex addressMutex;
    std::map<std::string, WalletAddress> addresses;
    uint32_t nextReceiveIndex;
    uint32_t nextChangeIndex;

    // Transaction tracking
    mutable std::mutex txMutex;
    std::map<crypto::Hash256, WalletTransaction> transactions;

    // UTXO database
    std::shared_ptr<storage::UTXODatabase> utxoDb;

    // Derivation path: m/44'/9999'/account'/change/index
    static constexpr uint32_t PURPOSE = 44 | 0x80000000;      // 44' (hardened)
    static constexpr uint32_t COIN_TYPE = 9999 | 0x80000000;  // 9999' (hardened) for UBU

    Impl(const crypto::ExtendedKey& master, uint32_t account)
        : masterKey(master), accountIndex(account), nextReceiveIndex(0), nextChangeIndex(0) {}
};

// ============================================================================
// Wallet Implementation
// ============================================================================

Wallet::Wallet(const crypto::ExtendedKey& masterKey, uint32_t accountIndex)
    : impl_(std::make_unique<Impl>(masterKey, accountIndex)) {}

Wallet::~Wallet() = default;

std::unique_ptr<Wallet> Wallet::createFromMnemonic(const std::string& mnemonic,
                                                     const std::string& passphrase,
                                                     uint32_t accountIndex) {
    // Create seed from mnemonic
    crypto::Seed seed = crypto::Seed::fromMnemonic(mnemonic, passphrase);

    // Derive master key
    auto masterKey = crypto::ExtendedKey::fromSeed(seed);

    auto wallet = std::unique_ptr<Wallet>(new Wallet(masterKey, accountIndex));
    spdlog::info("Wallet created from mnemonic");

    return wallet;
}

std::pair<std::unique_ptr<Wallet>, std::string> Wallet::createNew(uint32_t wordCount,
                                                                    uint32_t accountIndex) {
    // Generate new mnemonic
    auto mnemonic = crypto::Seed::generateMnemonic(wordCount);

    // Create wallet
    auto wallet = createFromMnemonic(mnemonic, "", accountIndex);

    spdlog::info("New wallet created with {}-word mnemonic", wordCount);

    return {std::move(wallet), mnemonic};
}

std::unique_ptr<Wallet> Wallet::loadFromFile(const std::string& filename,
                                               const std::string& password) {
    // In production, this would decrypt and deserialize wallet data
    // For now, return nullptr as placeholder
    spdlog::warn("Wallet::loadFromFile not fully implemented");
    return nullptr;
}

bool Wallet::saveToFile(const std::string& filename, const std::string& password) {
    // In production, this would encrypt and serialize wallet data
    // For now, just create a placeholder file
    std::ofstream file(filename);
    if (!file) {
        return false;
    }

    file << "# Ubuntu Blockchain Wallet\n";
    file << "# Encrypted with password\n";
    file.close();

    spdlog::info("Wallet saved to {}", filename);
    return true;
}

std::string Wallet::getNewAddress(const std::string& label, AddressType type) {
    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    uint32_t index = impl_->nextReceiveIndex++;

    // Derive key
    auto keyPair = deriveKey(false, index);

    // Create address
    std::string address = createAddress(keyPair.publicKey, type);

    // Store address info
    WalletAddress addrInfo;
    addrInfo.address = address;
    addrInfo.publicKey = keyPair.publicKey;
    addrInfo.label = label;
    addrInfo.type = type;
    addrInfo.index = index;
    addrInfo.isChange = false;

    impl_->addresses[address] = addrInfo;

    spdlog::info("Generated new address: {} (index: {})", address, index);

    return address;
}

std::string Wallet::getNewChangeAddress() {
    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    uint32_t index = impl_->nextChangeIndex++;

    // Derive change key
    auto keyPair = deriveKey(true, index);

    // Create address
    std::string address = createAddress(keyPair.publicKey, AddressType::P2PKH);

    // Store address info
    WalletAddress addrInfo;
    addrInfo.address = address;
    addrInfo.publicKey = keyPair.publicKey;
    addrInfo.label = "";
    addrInfo.type = AddressType::P2PKH;
    addrInfo.index = index;
    addrInfo.isChange = true;

    impl_->addresses[address] = addrInfo;

    spdlog::debug("Generated new change address: {} (index: {})", address, index);

    return address;
}

std::vector<WalletAddress> Wallet::getAddresses(bool includeChange) const {
    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    std::vector<WalletAddress> result;
    for (const auto& [addr, info] : impl_->addresses) {
        if (!info.isChange || includeChange) {
            result.push_back(info);
        }
    }

    return result;
}

std::optional<crypto::PrivateKey> Wallet::getPrivateKey(const std::string& address) const {
    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    auto it = impl_->addresses.find(address);
    if (it == impl_->addresses.end()) {
        return std::nullopt;
    }

    const auto& addrInfo = it->second;

    // Re-derive private key
    auto keyPair = deriveKey(addrInfo.isChange, addrInfo.index);

    return keyPair.privateKey;
}

uint64_t Wallet::getBalance(uint32_t minConfirmations) const {
    if (!impl_->utxoDb) {
        spdlog::warn("UTXO database not set");
        return 0;
    }

    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    uint64_t balance = 0;

    for (const auto& [addr, addrInfo] : impl_->addresses) {
        // Get UTXOs for this address
        // In production, we would check confirmations
        // For now, simplified implementation
        auto scriptPubKey = addrInfo.publicKey.toScriptPubKey();
        balance += impl_->utxoDb->getBalance(scriptPubKey);
    }

    return balance;
}

uint64_t Wallet::getUnconfirmedBalance() const {
    // Return balance with 0 confirmations
    return getBalance(0);
}

std::vector<WalletTransaction> Wallet::listTransactions(size_t count, size_t skip) const {
    std::lock_guard<std::mutex> lock(impl_->txMutex);

    std::vector<WalletTransaction> result;
    result.reserve(impl_->transactions.size());

    for (const auto& [hash, wtx] : impl_->transactions) {
        result.push_back(wtx);
    }

    // Sort by timestamp (newest first)
    std::sort(result.begin(), result.end(),
              [](const WalletTransaction& a, const WalletTransaction& b) {
                  return a.timestamp > b.timestamp;
              });

    // Apply skip and count
    if (skip >= result.size()) {
        return {};
    }

    result.erase(result.begin(), result.begin() + skip);

    if (result.size() > count) {
        result.resize(count);
    }

    return result;
}

std::optional<WalletTransaction> Wallet::getTransaction(const crypto::Hash256& txHash) const {
    std::lock_guard<std::mutex> lock(impl_->txMutex);

    auto it = impl_->transactions.find(txHash);
    if (it != impl_->transactions.end()) {
        return it->second;
    }

    return std::nullopt;
}

core::Transaction Wallet::createTransaction(const std::map<std::string, uint64_t>& recipients,
                                             uint64_t feeRate,
                                             bool subtractFee) {
    if (!impl_->utxoDb) {
        throw std::runtime_error("UTXO database not set");
    }

    // Calculate total output amount
    uint64_t totalOutput = 0;
    for (const auto& [addr, amount] : recipients) {
        totalOutput += amount;
    }

    // Estimate fee (will be refined after coin selection)
    size_t outputCount = recipients.size() + 1;  // +1 for change
    uint64_t estimatedFee = estimateTransactionFee(1, outputCount, feeRate);

    // Select coins
    uint64_t targetAmount = totalOutput + estimatedFee;
    auto [selectedUtxos, changeAmount] = selectCoins(targetAmount, feeRate);

    if (selectedUtxos.empty()) {
        throw std::runtime_error("Insufficient funds");
    }

    // Recalculate fee with actual input count
    uint64_t actualFee = estimateTransactionFee(selectedUtxos.size(), outputCount, feeRate);

    // Build transaction
    core::Transaction tx;
    tx.version = 1;
    tx.lockTime = 0;

    // Add inputs
    for (const auto& utxo : selectedUtxos) {
        core::TxInput input;
        input.previousOutput = utxo.outpoint;
        input.sequence = 0xFFFFFFFE;  // Enable RBF
        tx.inputs.push_back(input);
    }

    // Add outputs
    for (const auto& [address, amount] : recipients) {
        core::TxOutput output;
        output.value = amount;

        // In production, decode address to script
        // For now, simplified
        output.scriptPubKey = {};

        tx.outputs.push_back(output);
    }

    // Add change output if needed
    uint64_t totalInput = 0;
    for (const auto& utxo : selectedUtxos) {
        totalInput += utxo.output.value;
    }

    uint64_t change = totalInput - totalOutput - actualFee;
    if (change > 546) {  // Dust threshold
        std::string changeAddr = getNewChangeAddress();

        core::TxOutput changeOutput;
        changeOutput.value = change;
        changeOutput.scriptPubKey = {};  // Simplified

        tx.outputs.push_back(changeOutput);
    }

    // Sign transaction
    if (!signTransaction(tx)) {
        throw std::runtime_error("Failed to sign transaction");
    }

    spdlog::info("Created transaction: {} inputs, {} outputs, {} sat fee", tx.inputs.size(),
                 tx.outputs.size(), actualFee);

    return tx;
}

bool Wallet::signTransaction(core::Transaction& tx) {
    if (!impl_->utxoDb) {
        return false;
    }

    for (size_t i = 0; i < tx.inputs.size(); ++i) {
        auto& input = tx.inputs[i];

        // Get UTXO being spent
        auto utxo = impl_->utxoDb->getUTXO(input.previousOutput);
        if (!utxo) {
            spdlog::error("UTXO not found for input {}", i);
            return false;
        }

        // Find corresponding address
        // In production, extract address from scriptPubKey
        // For now, simplified - assume we can sign all inputs
        std::lock_guard<std::mutex> lock(impl_->addressMutex);

        // Sign the input
        // In production, this would:
        // 1. Extract address from UTXO scriptPubKey
        // 2. Find private key for that address
        // 3. Create signature hash
        // 4. Sign with ECDSA
        // 5. Build scriptSig

        // Simplified placeholder
        input.scriptSig = {0x00};  // Placeholder signature
    }

    return true;
}

bool Wallet::addTransaction(const core::Transaction& tx, uint32_t blockHeight) {
    std::lock_guard<std::mutex> lock(impl_->txMutex);

    auto txHash = tx.getHash();

    // Check if already exists
    if (impl_->transactions.find(txHash) != impl_->transactions.end()) {
        return false;
    }

    // Calculate net amount
    int64_t netAmount = 0;

    // Check outputs
    for (const auto& output : tx.outputs) {
        // In production, check if output is ours
        // For now, simplified
    }

    // Create wallet transaction
    WalletTransaction wtx;
    wtx.tx = tx;
    wtx.txHash = txHash;
    wtx.amount = netAmount;
    wtx.blockHeight = blockHeight;
    wtx.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
    wtx.comment = "";

    impl_->transactions[txHash] = wtx;

    spdlog::info("Added transaction {} to wallet", txHash.toHex());

    return true;
}

void Wallet::rescan(uint32_t fromHeight) {
    spdlog::info("Rescanning blockchain from height {}", fromHeight);

    // In production, this would scan the blockchain for transactions
    // involving our addresses and update wallet state
    // For now, placeholder implementation
}

void Wallet::setUtxoDatabase(std::shared_ptr<storage::UTXODatabase> utxoDb) {
    impl_->utxoDb = utxoDb;
}

crypto::KeyPair Wallet::deriveKey(bool isChange, uint32_t index) const {
    // BIP-44 path: m/44'/9999'/account'/change/index
    auto purposeKey = impl_->masterKey.deriveChild(Impl::PURPOSE);
    auto coinTypeKey = purposeKey.deriveChild(Impl::COIN_TYPE);
    auto accountKey = coinTypeKey.deriveChild(impl_->accountIndex | 0x80000000);
    auto changeKey = accountKey.deriveChild(isChange ? 1 : 0);
    auto addressKey = changeKey.deriveChild(index);

    return addressKey.toKeyPair();
}

std::string Wallet::createAddress(const crypto::PublicKey& publicKey, AddressType type) const {
    switch (type) {
        case AddressType::P2PKH: {
            // P2PKH address (legacy)
            auto pubKeyHash = publicKey.toHash160();
            auto addressBytes = pubKeyHash.toBytes();
            addressBytes.insert(addressBytes.begin(), 0x00);  // Mainnet prefix
            return Base58::encodeCheck(addressBytes);
        }

        case AddressType::BECH32: {
            // Bech32 address
            auto pubKeyHash = publicKey.toHash160();
            return crypto::Bech32::encode("ubu", pubKeyHash.toBytes());
        }

        case AddressType::P2SH:
        default:
            throw std::runtime_error("Unsupported address type");
    }
}

std::pair<std::vector<core::UTXO>, uint64_t> Wallet::selectCoins(uint64_t targetAmount,
                                                                   uint64_t feeRate) const {
    if (!impl_->utxoDb) {
        return {{}, 0};
    }

    std::vector<core::UTXO> selected;
    uint64_t totalSelected = 0;

    std::lock_guard<std::mutex> lock(impl_->addressMutex);

    // Collect all available UTXOs
    std::vector<core::UTXO> availableUtxos;
    for (const auto& [addr, addrInfo] : impl_->addresses) {
        // In production, get UTXOs for each address
        // For now, simplified
    }

    // Simple coin selection: largest first
    std::sort(availableUtxos.begin(), availableUtxos.end(),
              [](const core::UTXO& a, const core::UTXO& b) {
                  return a.output.value > b.output.value;
              });

    for (const auto& utxo : availableUtxos) {
        selected.push_back(utxo);
        totalSelected += utxo.output.value;

        // Recalculate fee with current input count
        uint64_t fee = estimateTransactionFee(selected.size(), 2, feeRate);

        if (totalSelected >= targetAmount + fee) {
            uint64_t change = totalSelected - targetAmount - fee;
            return {selected, change};
        }
    }

    // Insufficient funds
    return {{}, 0};
}

uint64_t Wallet::estimateTransactionFee(size_t inputCount, size_t outputCount,
                                         uint64_t feeRate) const {
    // Estimate transaction size
    // Each input: ~148 bytes (P2PKH)
    // Each output: ~34 bytes (P2PKH)
    // Overhead: ~10 bytes
    size_t estimatedSize = 10 + (inputCount * 148) + (outputCount * 34);

    return estimatedSize * feeRate;
}

}  // namespace wallet
}  // namespace ubuntu
