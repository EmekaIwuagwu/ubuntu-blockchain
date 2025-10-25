#include "ubuntu/core/merkle.h"

#include <algorithm>
#include <stdexcept>

namespace ubuntu {
namespace core {

// ============================================================================
// MerkleTree Implementation
// ============================================================================

MerkleTree::MerkleTree(const std::vector<crypto::Hash256>& txHashes) {
    if (txHashes.empty()) {
        throw std::invalid_argument("Cannot create Merkle tree from empty transaction list");
    }

    // Build the tree level by level
    levels_.push_back(txHashes);

    while (levels_.back().size() > 1) {
        const auto& currentLevel = levels_.back();
        std::vector<crypto::Hash256> nextLevel;

        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            if (i + 1 < currentLevel.size()) {
                // Hash two nodes together
                nextLevel.push_back(hashNodes(currentLevel[i], currentLevel[i + 1]));
            } else {
                // Odd number of nodes: duplicate the last one
                nextLevel.push_back(hashNodes(currentLevel[i], currentLevel[i]));
            }
        }

        levels_.push_back(nextLevel);
    }

    // Root is the single element in the last level
    root_ = levels_.back()[0];
}

std::vector<crypto::Hash256> MerkleTree::generateProof(size_t txIndex) const {
    if (levels_.empty()) {
        throw std::logic_error("Merkle tree not initialized");
    }

    if (txIndex >= levels_[0].size()) {
        throw std::out_of_range("Transaction index out of range");
    }

    std::vector<crypto::Hash256> proof;
    size_t index = txIndex;

    // Traverse from leaf to root, collecting sibling hashes
    for (size_t level = 0; level < levels_.size() - 1; ++level) {
        const auto& currentLevel = levels_[level];

        // Find sibling index
        size_t siblingIndex;
        if (index % 2 == 0) {
            // Left node: sibling is on the right
            siblingIndex = index + 1;
            if (siblingIndex >= currentLevel.size()) {
                // No sibling (odd number): use same node
                siblingIndex = index;
            }
        } else {
            // Right node: sibling is on the left
            siblingIndex = index - 1;
        }

        proof.push_back(currentLevel[siblingIndex]);

        // Move to parent in next level
        index /= 2;
    }

    return proof;
}

bool MerkleTree::verifyProof(const crypto::Hash256& txHash,
                             size_t txIndex,
                             const std::vector<crypto::Hash256>& proof,
                             const crypto::Hash256& root) {
    crypto::Hash256 currentHash = txHash;
    size_t index = txIndex;

    for (const auto& siblingHash : proof) {
        if (index % 2 == 0) {
            // Current node is left child
            currentHash = hashNodes(currentHash, siblingHash);
        } else {
            // Current node is right child
            currentHash = hashNodes(siblingHash, currentHash);
        }

        index /= 2;
    }

    return currentHash == root;
}

crypto::Hash256 MerkleTree::computeRoot(const std::vector<crypto::Hash256>& txHashes) {
    if (txHashes.empty()) {
        return crypto::Hash256::zero();
    }

    if (txHashes.size() == 1) {
        return txHashes[0];
    }

    std::vector<crypto::Hash256> currentLevel = txHashes;

    while (currentLevel.size() > 1) {
        std::vector<crypto::Hash256> nextLevel;

        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            if (i + 1 < currentLevel.size()) {
                nextLevel.push_back(hashNodes(currentLevel[i], currentLevel[i + 1]));
            } else {
                // Duplicate last node if odd number
                nextLevel.push_back(hashNodes(currentLevel[i], currentLevel[i]));
            }
        }

        currentLevel = std::move(nextLevel);
    }

    return currentLevel[0];
}

crypto::Hash256 MerkleTree::hashNodes(const crypto::Hash256& left,
                                      const crypto::Hash256& right) {
    // Concatenate the two hashes
    std::vector<uint8_t> data;
    data.reserve(64);
    data.insert(data.end(), left.begin(), left.end());
    data.insert(data.end(), right.begin(), right.end());

    // Double SHA-256
    return crypto::sha256d(std::span<const uint8_t>(data.data(), data.size()));
}

// ============================================================================
// MerkleBlock Implementation
// ============================================================================

std::vector<crypto::Hash256> MerkleBlock::extractMatches() const {
    std::vector<crypto::Hash256> matches;

    // This is a simplified implementation
    // Full implementation would parse the partial Merkle tree
    // using the flags to determine which hashes are transactions

    size_t hashIndex = 0;
    size_t flagIndex = 0;

    // Simplified: just return all hashes marked as leaves
    for (size_t i = 0; i < flags.size() && hashIndex < hashes.size(); ++i) {
        if (flags[i]) {
            // This is a matched transaction
            matches.push_back(hashes[hashIndex]);
        }
        hashIndex++;
    }

    return matches;
}

}  // namespace core
}  // namespace ubuntu
