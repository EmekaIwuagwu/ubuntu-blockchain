#pragma once

#include "ubuntu/crypto/hash.h"

#include <optional>
#include <vector>

namespace ubuntu {
namespace core {

/**
 * @brief Merkle tree for efficient transaction verification
 *
 * A Merkle tree is a binary tree where each leaf is a transaction hash
 * and each internal node is the hash of its two children. This allows
 * SPV (Simplified Payment Verification) clients to verify a transaction
 * is in a block without downloading all transactions.
 */
class MerkleTree {
public:
    /**
     * @brief Build a Merkle tree from transaction hashes
     *
     * @param txHashes Vector of transaction hashes (leaves)
     */
    explicit MerkleTree(const std::vector<crypto::Hash256>& txHashes);

    /**
     * @brief Get the Merkle root hash
     *
     * @return Root hash of the tree
     */
    crypto::Hash256 getRoot() const { return root_; }

    /**
     * @brief Generate a Merkle proof for a transaction
     *
     * A Merkle proof is a list of hashes needed to reconstruct the path
     * from a leaf to the root, allowing verification without the full tree.
     *
     * @param txIndex Index of the transaction in the block
     * @return Vector of sibling hashes from leaf to root
     */
    std::vector<crypto::Hash256> generateProof(size_t txIndex) const;

    /**
     * @brief Verify a Merkle proof
     *
     * @param txHash Transaction hash to verify
     * @param txIndex Index of the transaction in the block
     * @param proof Merkle proof (sibling hashes)
     * @param root Expected Merkle root
     * @return true if proof is valid
     */
    static bool verifyProof(const crypto::Hash256& txHash,
                            size_t txIndex,
                            const std::vector<crypto::Hash256>& proof,
                            const crypto::Hash256& root);

    /**
     * @brief Compute Merkle root directly from transaction hashes
     *
     * @param txHashes Vector of transaction hashes
     * @return Merkle root hash
     */
    static crypto::Hash256 computeRoot(const std::vector<crypto::Hash256>& txHashes);

private:
    crypto::Hash256 root_;
    std::vector<std::vector<crypto::Hash256>> levels_;  // All tree levels for proof generation

    /**
     * @brief Hash two nodes together (Merkle tree node hash)
     *
     * Computes SHA-256d(left || right)
     */
    static crypto::Hash256 hashNodes(const crypto::Hash256& left,
                                     const crypto::Hash256& right);
};

/**
 * @brief Merkle block for SPV verification
 *
 * Contains a block header and partial Merkle tree with only the
 * transactions relevant to an SPV client.
 */
struct MerkleBlock {
    // Block header
    crypto::Hash256 blockHash;
    uint32_t height;

    // Partial Merkle tree data
    std::vector<crypto::Hash256> hashes;    // Hashes in depth-first order
    std::vector<bool> flags;                 // Flags indicating node type
    uint32_t totalTransactions;

    /**
     * @brief Extract matched transaction hashes from the Merkle block
     *
     * @return Vector of transaction hashes that matched the filter
     */
    std::vector<crypto::Hash256> extractMatches() const;
};

}  // namespace core
}  // namespace ubuntu
