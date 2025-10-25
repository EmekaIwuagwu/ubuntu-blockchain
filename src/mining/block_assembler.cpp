#include "ubuntu/mining/block_assembler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>

namespace ubuntu {
namespace mining {

// ============================================================================
// BlockAssembler Implementation
// ============================================================================

BlockAssembler::BlockAssembler(std::shared_ptr<core::Blockchain> blockchain,
                               std::shared_ptr<mempool::Mempool> mempool,
                               std::shared_ptr<storage::UTXODatabase> utxoDb,
                               const consensus::ChainParams& params)
    : blockchain_(blockchain),
      mempool_(mempool),
      utxoDb_(utxoDb),
      params_(params),
      maxBlockSize_(params.maxBlockSize),
      maxBlockWeight_(params.maxBlockSize * 4),  // Weight = size * 4
      minFeeRate_(1),                             // 1 sat/byte minimum
      lastStats_{} {}

BlockAssembler::~BlockAssembler() = default;

std::optional<BlockTemplate> BlockAssembler::createBlockTemplate(
    const std::vector<uint8_t>& minerScriptPubKey) {

    // Get current blockchain state
    auto tip = blockchain_->getTip();
    if (!tip) {
        spdlog::error("Cannot create block template: no blockchain tip");
        return std::nullopt;
    }

    auto state = blockchain_->getState();
    uint32_t height = state.height + 1;

    spdlog::info("Creating block template for height {}", height);

    // Select transactions from mempool
    auto [transactions, totalFees] = selectTransactions(maxBlockSize_, maxBlockWeight_);

    spdlog::info("Selected {} transactions with {} sat in fees", transactions.size(), totalFees);

    // Create coinbase transaction
    auto coinbase = createCoinbaseTransaction(height, minerScriptPubKey, totalFees);

    // Build block
    BlockTemplate blockTemplate;
    blockTemplate.block.header.version = 1;
    blockTemplate.block.header.previousBlockHash = tip->getHash();
    blockTemplate.block.header.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
    blockTemplate.block.header.bits = state.target;
    blockTemplate.block.header.nonce = 0;

    // Add transactions (coinbase first)
    blockTemplate.block.transactions.push_back(coinbase);
    blockTemplate.block.transactions.insert(blockTemplate.block.transactions.end(),
                                            transactions.begin(), transactions.end());

    // Calculate merkle root
    std::vector<crypto::Hash256> txHashes;
    for (const auto& tx : blockTemplate.block.transactions) {
        txHashes.push_back(tx.getHash());
    }
    core::MerkleTree merkleTree(txHashes);
    blockTemplate.block.header.merkleRoot = merkleTree.getRoot();

    // Set fees
    blockTemplate.totalFees = totalFees;
    blockTemplate.txFees.push_back(0);  // Coinbase has no fee
    for (const auto& tx : transactions) {
        blockTemplate.txFees.push_back(getTransactionFee(tx));
    }

    // Calculate block weight and sigops
    blockTemplate.weight = 0;
    blockTemplate.sigOps = 0;
    for (const auto& tx : blockTemplate.block.transactions) {
        blockTemplate.weight += calculateTxWeight(tx);
        blockTemplate.sigOps += countSigOps(tx);
    }

    // Update statistics
    lastStats_.txIncluded = transactions.size();
    lastStats_.totalFees = totalFees;
    lastStats_.blockSize = blockTemplate.block.getSize();
    lastStats_.blockWeight = blockTemplate.weight;
    lastStats_.sigOps = blockTemplate.sigOps;

    spdlog::info("Block template created: {} tx, {} bytes, {} sat fees", transactions.size() + 1,
                 lastStats_.blockSize, totalFees);

    return blockTemplate;
}

bool BlockAssembler::updateBlockTemplate(BlockTemplate& blockTemplate) {
    // Get miner script from coinbase
    if (blockTemplate.block.transactions.empty()) {
        return false;
    }

    auto& coinbase = blockTemplate.block.transactions[0];
    if (coinbase.outputs.empty()) {
        return false;
    }

    auto minerScriptPubKey = coinbase.outputs[0].scriptPubKey;

    // Create new template
    auto newTemplate = createBlockTemplate(minerScriptPubKey);
    if (!newTemplate) {
        return false;
    }

    // Update timestamp and nonce
    newTemplate->block.header.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
    newTemplate->block.header.nonce = blockTemplate.block.header.nonce;

    blockTemplate = *newTemplate;
    return true;
}

void BlockAssembler::setMaxBlockSize(uint64_t maxSize) {
    maxBlockSize_ = maxSize;
}

void BlockAssembler::setMaxBlockWeight(uint64_t maxWeight) {
    maxBlockWeight_ = maxWeight;
}

void BlockAssembler::setMinFeeRate(uint64_t minFeeRate) {
    minFeeRate_ = minFeeRate;
}

BlockAssembler::AssemblyStats BlockAssembler::getLastAssemblyStats() const {
    return lastStats_;
}

core::Transaction BlockAssembler::createCoinbaseTransaction(
    uint32_t height,
    const std::vector<uint8_t>& minerScriptPubKey,
    uint64_t totalFees) {

    core::Transaction coinbase;
    coinbase.version = 1;
    coinbase.lockTime = 0;

    // Coinbase input
    core::TxInput input;
    input.previousOutput.txHash = crypto::Hash256();  // Zero hash
    input.previousOutput.vout = 0xFFFFFFFF;
    input.sequence = 0xFFFFFFFF;

    // Coinbase script sig (height + arbitrary data)
    input.scriptSig.resize(4);
    input.scriptSig[0] = height & 0xFF;
    input.scriptSig[1] = (height >> 8) & 0xFF;
    input.scriptSig[2] = (height >> 16) & 0xFF;
    input.scriptSig[3] = (height >> 24) & 0xFF;

    coinbase.inputs.push_back(input);

    // Coinbase output (block reward + fees)
    core::TxOutput output;
    output.value = calculateBlockReward(height) + totalFees;
    output.scriptPubKey = minerScriptPubKey;

    coinbase.outputs.push_back(output);

    return coinbase;
}

uint64_t BlockAssembler::calculateBlockReward(uint32_t height) const {
    // Initial reward: 50 UBU = 50 * 10^8 satoshis
    uint64_t reward = 50 * consensus::Constants::COIN;

    // Halving every 210,000 blocks (approximately 4 years)
    uint32_t halvings = height / 210000;

    // Reward goes to zero after 64 halvings
    if (halvings >= 64) {
        return 0;
    }

    // Right shift is equivalent to division by 2^halvings
    return reward >> halvings;
}

std::pair<std::vector<core::Transaction>, uint64_t> BlockAssembler::selectTransactions(
    uint64_t maxSize,
    uint64_t maxWeight) {

    std::vector<core::Transaction> selected;
    uint64_t totalFees = 0;
    uint64_t currentSize = 0;
    uint64_t currentWeight = 0;
    uint32_t currentSigOps = 0;

    // Reserve space for coinbase
    const uint64_t COINBASE_SIZE = 200;  // Approximate
    currentSize += COINBASE_SIZE;
    currentWeight += COINBASE_SIZE * 4;

    // Get transactions from mempool sorted by fee rate
    auto candidates = mempool_->selectTransactionsForMining(maxSize, maxWeight);

    size_t excluded = 0;

    for (const auto& tx : candidates) {
        // Check if transaction can be included
        if (!canIncludeTransaction(tx)) {
            excluded++;
            continue;
        }

        uint64_t txSize = tx.getSize();
        uint32_t txWeight = calculateTxWeight(tx);
        uint32_t txSigOps = countSigOps(tx);
        uint64_t txFee = getTransactionFee(tx);

        // Check size limits
        if (currentSize + txSize > maxSize) {
            excluded++;
            continue;
        }

        // Check weight limits
        if (currentWeight + txWeight > maxWeight) {
            excluded++;
            continue;
        }

        // Check sigop limits (max 80,000 per block)
        if (currentSigOps + txSigOps > 80000) {
            excluded++;
            continue;
        }

        // Check minimum fee rate
        uint64_t feeRate = txSize > 0 ? (txFee / txSize) : 0;
        if (feeRate < minFeeRate_) {
            excluded++;
            continue;
        }

        // Add transaction
        selected.push_back(tx);
        totalFees += txFee;
        currentSize += txSize;
        currentWeight += txWeight;
        currentSigOps += txSigOps;
    }

    lastStats_.txExcluded = excluded;

    return {selected, totalFees};
}

uint32_t BlockAssembler::calculateTxWeight(const core::Transaction& tx) const {
    // In Bitcoin, weight = size * 4 for non-witness transactions
    // For simplicity, we use the same formula
    return tx.getSize() * 4;
}

uint32_t BlockAssembler::countSigOps(const core::Transaction& tx) const {
    uint32_t sigOps = 0;

    // Count signature operations in inputs
    for (const auto& input : tx.inputs) {
        // Each input typically has 1 signature
        // In a full implementation, we would parse the script
        if (!input.isCoinbase()) {
            sigOps += 1;
        }
    }

    // Count signature operations in outputs
    for (const auto& output : tx.outputs) {
        // P2PKH outputs require 1 signature to spend
        // In a full implementation, we would parse the script
        sigOps += 1;
    }

    return sigOps;
}

bool BlockAssembler::canIncludeTransaction(const core::Transaction& tx) const {
    // Basic validation
    if (tx.inputs.empty() || tx.outputs.empty()) {
        return false;
    }

    // Check if transaction is already in a block
    auto txHash = tx.getHash();
    if (!mempool_->hasTransaction(txHash)) {
        return false;
    }

    // Check all inputs are available in UTXO set or mempool
    for (const auto& input : tx.inputs) {
        if (input.isCoinbase()) {
            continue;
        }

        // Check UTXO database
        if (!utxoDb_->hasUTXO(input.previousOutput)) {
            // Could be spending from another mempool tx
            // In a full implementation, we would check mempool dependencies
            return false;
        }
    }

    return true;
}

uint64_t BlockAssembler::getTransactionFee(const core::Transaction& tx) const {
    uint64_t inputValue = 0;
    uint64_t outputValue = 0;

    // Sum inputs
    for (const auto& input : tx.inputs) {
        if (input.isCoinbase()) {
            continue;
        }

        auto utxo = utxoDb_->getUTXO(input.previousOutput);
        if (utxo) {
            inputValue += utxo->output.value;
        }
    }

    // Sum outputs
    for (const auto& output : tx.outputs) {
        outputValue += output.value;
    }

    // Fee = inputs - outputs
    if (inputValue > outputValue) {
        return inputValue - outputValue;
    }

    return 0;
}

}  // namespace mining
}  // namespace ubuntu
