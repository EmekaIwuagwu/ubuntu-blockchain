#include "ubuntu/core/chain.h"
#include "ubuntu/consensus/pow.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

namespace ubuntu {
namespace core {

// ============================================================================
// Blockchain Implementation
// ============================================================================

Blockchain::Blockchain(const consensus::ChainParams& params)
    : params_(params) {}

Blockchain::~Blockchain() = default;

bool Blockchain::initialize() {
    spdlog::info("Initializing blockchain for network: {}", params_.networkName);

    // Create genesis block
    Block genesis = createGenesisBlock();
    genesis.header.timestamp = params_.genesisTimestamp;
    genesis.header.difficulty = params_.genesisDifficulty;
    genesis.header.nonce = params_.genesisNonce;

    // Add coinbase transaction for genesis
    Transaction coinbaseTx;
    coinbaseTx.version = 1;

    // Coinbase input (null previous output)
    TxInput coinbaseInput;
    coinbaseInput.previousOutput = TxOutpoint(crypto::Hash256::zero(), 0xFFFFFFFF);
    coinbaseInput.scriptSig = {'U', 'b', 'u', 'n', 't', 'u', ' ', 'B', 'l', 'o', 'c', 'k', 'c', 'h', 'a', 'i', 'n'};
    coinbaseInput.sequence = 0xFFFFFFFF;
    coinbaseTx.inputs.push_back(coinbaseInput);

    // Coinbase output
    TxOutput coinbaseOutput;
    coinbaseOutput.value = params_.initialBlockReward;
    coinbaseOutput.scriptPubKey = {0x76, 0xa9, 0x14};  // OP_DUP OP_HASH160 ... (simplified)
    coinbaseTx.outputs.push_back(coinbaseOutput);

    genesis.transactions.push_back(coinbaseTx);

    // Calculate Merkle root
    genesis.header.merkleRoot = genesis.calculateMerkleRoot();

    // Mine genesis block if needed (for regtest/testnet)
    if (params_.networkType != consensus::NetworkType::MAINNET) {
        spdlog::info("Mining genesis block...");
        consensus::Miner::mineBlock(genesis);
        spdlog::info("Genesis block mined! Nonce: {}", genesis.header.nonce);
    }

    auto genesisHash = genesis.calculateHash();
    spdlog::info("Genesis block hash: {}", genesisHash.toHex());

    // Create genesis block index
    auto genesisIndex = std::make_unique<BlockIndex>(genesis);
    genesisIndex->valid = true;
    genesisIndex->chainWork = consensus::PoW::getBlockWork(genesis.header.difficulty);

    // Store genesis block
    blocks_[genesisHash] = genesis;
    blockIndex_[genesisHash] = std::move(genesisIndex);
    heightIndex_[0] = genesisHash;

    // Set chain state
    state_.bestBlockHash = genesisHash;
    state_.height = 0;
    state_.totalWork = blockIndex_[genesisHash]->chainWork;
    state_.timestamp = genesis.header.timestamp;
    state_.difficulty = genesis.header.difficulty;

    spdlog::info("Blockchain initialized successfully");
    return true;
}

bool Blockchain::addBlock(const Block& block) {
    auto blockHash = block.calculateHash();

    // Check if block already exists
    if (hasBlock(blockHash)) {
        spdlog::debug("Block {} already exists", blockHash.toHex());
        return true;
    }

    // Get previous block index
    auto prevIndex = getBlockIndex(block.header.previousBlockHash);
    if (!prevIndex) {
        spdlog::warn("Previous block not found for {}", blockHash.toHex());
        return false;
    }

    // Validate block headers
    if (!validateBlockHeaders(block, prevIndex)) {
        spdlog::warn("Block header validation failed for {}", blockHash.toHex());
        return false;
    }

    // Validate block transactions
    if (!validateBlockTransactions(block)) {
        spdlog::warn("Block transaction validation failed for {}", blockHash.toHex());
        return false;
    }

    // Create block index
    auto newIndex = std::make_unique<BlockIndex>(block);
    newIndex->previous = prevIndex;
    newIndex->height = prevIndex->height + 1;
    newIndex->chainWork = prevIndex->chainWork +
                          consensus::PoW::getBlockWork(block.header.difficulty);
    newIndex->valid = true;

    // Add to previous block's children
    prevIndex->children.push_back(newIndex.get());

    // Store block
    blocks_[blockHash] = block;
    auto* indexPtr = newIndex.get();
    blockIndex_[blockHash] = std::move(newIndex);

    spdlog::info("Added block {} at height {}", blockHash.toHex(), indexPtr->height);

    // Check if reorganization is needed
    if (shouldReorganize(indexPtr)) {
        spdlog::info("Reorganizing chain to new tip");
        return reorganize(indexPtr);
    } else if (indexPtr->chainWork > state_.totalWork) {
        // New tip on main chain
        setChainTip(indexPtr);
    }

    return true;
}

std::optional<Block> Blockchain::getBlock(const crypto::Hash256& hash) const {
    auto it = blocks_.find(hash);
    if (it != blocks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Block> Blockchain::getBlockByHeight(uint32_t height) const {
    auto it = heightIndex_.find(height);
    if (it != heightIndex_.end()) {
        return getBlock(it->second);
    }
    return std::nullopt;
}

std::optional<Block> Blockchain::getTip() const {
    return getBlock(state_.bestBlockHash);
}

bool Blockchain::hasBlock(const crypto::Hash256& hash) const {
    return blockIndex_.find(hash) != blockIndex_.end();
}

bool Blockchain::validateBlock(const Block& block) const {
    // Basic validation
    if (!block.validate()) {
        return false;
    }

    // PoW validation
    if (!consensus::PoW::verifyBlockPoW(block)) {
        return false;
    }

    return true;
}

BlockIndex* Blockchain::getBlockIndex(const crypto::Hash256& hash) const {
    auto it = blockIndex_.find(hash);
    if (it != blockIndex_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<crypto::Hash256> Blockchain::getMainChain() const {
    std::vector<crypto::Hash256> chain;
    chain.reserve(state_.height + 1);

    for (uint32_t i = 0; i <= state_.height; ++i) {
        auto it = heightIndex_.find(i);
        if (it != heightIndex_.end()) {
            chain.push_back(it->second);
        }
    }

    return chain;
}

std::optional<crypto::Hash256> Blockchain::findCommonAncestor(
    const crypto::Hash256& hash1,
    const crypto::Hash256& hash2) const {

    auto* index1 = getBlockIndex(hash1);
    auto* index2 = getBlockIndex(hash2);

    if (!index1 || !index2) {
        return std::nullopt;
    }

    // Walk back to same height
    while (index1->height > index2->height) {
        index1 = index1->previous;
    }
    while (index2->height > index1->height) {
        index2 = index2->previous;
    }

    // Walk back together until common ancestor
    while (index1 && index2 && index1->hash != index2->hash) {
        index1 = index1->previous;
        index2 = index2->previous;
    }

    if (index1 && index2) {
        return index1->hash;
    }

    return std::nullopt;
}

void Blockchain::setChainTip(BlockIndex* newTip) {
    state_.bestBlockHash = newTip->hash;
    state_.height = newTip->height;
    state_.totalWork = newTip->chainWork;
    state_.timestamp = newTip->timestamp;
    state_.difficulty = newTip->difficulty;

    // Update height index for main chain
    heightIndex_[newTip->height] = newTip->hash;

    spdlog::info("New chain tip: {} at height {}", newTip->hash.toHex(), newTip->height);
}

bool Blockchain::reorganize(BlockIndex* newTip) {
    // Find common ancestor with current tip
    auto currentTip = getBlockIndex(state_.bestBlockHash);
    if (!currentTip) {
        return false;
    }

    // Find fork point
    std::vector<BlockIndex*> disconnect;
    std::vector<BlockIndex*> connect;

    auto* oldBlock = currentTip;
    auto* newBlock = newTip;

    // Build disconnect and connect lists
    while (oldBlock->height > newBlock->height) {
        disconnect.push_back(oldBlock);
        oldBlock = oldBlock->previous;
    }

    while (newBlock->height > oldBlock->height) {
        connect.push_back(newBlock);
        newBlock = newBlock->previous;
    }

    while (oldBlock != newBlock) {
        disconnect.push_back(oldBlock);
        connect.push_back(newBlock);
        oldBlock = oldBlock->previous;
        newBlock = newBlock->previous;
    }

    spdlog::info("Reorganizing: disconnecting {} blocks, connecting {} blocks",
                 disconnect.size(), connect.size());

    // Disconnect old blocks
    for (auto* index : disconnect) {
        auto block = getBlock(index->hash);
        if (!block || !disconnectBlock(*block)) {
            spdlog::error("Failed to disconnect block during reorg");
            return false;
        }
    }

    // Connect new blocks (reverse order)
    std::reverse(connect.begin(), connect.end());
    for (auto* index : connect) {
        auto block = getBlock(index->hash);
        if (!block || !connectBlock(*block)) {
            spdlog::error("Failed to connect block during reorg");
            // TODO: Rollback to previous state
            return false;
        }
    }

    // Set new tip
    setChainTip(newTip);

    return true;
}

bool Blockchain::connectBlock(const Block& block) {
    // Update UTXO set (would interact with UTXO database)
    // For now, just log
    spdlog::debug("Connected block {}", block.calculateHash().toHex());
    return true;
}

bool Blockchain::disconnectBlock(const Block& block) {
    // Revert UTXO set changes
    spdlog::debug("Disconnected block {}", block.calculateHash().toHex());
    return true;
}

uint32_t Blockchain::calculateNextDifficulty(const BlockIndex* prevIndex) const {
    if (!prevIndex) {
        return params_.genesisDifficulty;
    }

    // Check if retargeting is disabled (regtest)
    if (params_.noRetargeting) {
        return prevIndex->difficulty;
    }

    // Only adjust on interval
    if ((prevIndex->height + 1) % params_.difficultyAdjustmentInterval != 0) {
        return prevIndex->difficulty;
    }

    // Find the block at the start of the adjustment period
    auto* firstBlock = prevIndex;
    for (uint32_t i = 0; i < params_.difficultyAdjustmentInterval - 1 && firstBlock->previous; ++i) {
        firstBlock = firstBlock->previous;
    }

    // Calculate actual timespan
    uint32_t actualTimespan = prevIndex->timestamp - firstBlock->timestamp;
    uint32_t targetTimespan = params_.powTargetTimespan;

    // Calculate new difficulty
    return consensus::PoW::calculateNextDifficulty(
        prevIndex->difficulty,
        actualTimespan,
        targetTimespan
    );
}

bool Blockchain::validateBlockHeaders(const Block& block, const BlockIndex* prevIndex) const {
    // Check previous block hash
    if (block.header.previousBlockHash != prevIndex->hash) {
        return false;
    }

    // Check height
    if (block.header.height != prevIndex->height + 1) {
        return false;
    }

    // Check PoW
    if (!consensus::PoW::verifyBlockPoW(block)) {
        return false;
    }

    // Check difficulty
    uint32_t expectedDifficulty = calculateNextDifficulty(prevIndex);
    if (block.header.difficulty != expectedDifficulty) {
        // Allow minimum difficulty blocks on testnet if enabled
        if (!params_.allowMinDifficultyBlocks ||
            block.header.difficulty != consensus::Constants::MAX_PROOF_OF_WORK) {
            return false;
        }
    }

    // Check timestamp (not too far in the future)
    // Simplified: would check against current time + 2 hours

    return true;
}

bool Blockchain::validateBlockTransactions(const Block& block) const {
    if (block.transactions.empty()) {
        return false;
    }

    // First transaction must be coinbase
    if (!block.transactions[0].isCoinbase()) {
        return false;
    }

    // Only first can be coinbase
    for (size_t i = 1; i < block.transactions.size(); ++i) {
        if (block.transactions[i].isCoinbase()) {
            return false;
        }
    }

    // Validate Merkle root
    if (block.header.merkleRoot != block.calculateMerkleRoot()) {
        return false;
    }

    // TODO: Validate all transactions properly
    // - Check inputs exist
    // - Verify signatures
    // - Check no double spends
    // - Validate scripts

    return true;
}

bool Blockchain::shouldReorganize(const BlockIndex* newIndex) const {
    // Reorganize if new chain has more work
    return newIndex->chainWork > state_.totalWork;
}

}  // namespace core
}  // namespace ubuntu
