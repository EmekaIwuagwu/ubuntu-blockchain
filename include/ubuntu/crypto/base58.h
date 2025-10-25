#pragma once

#include <span>
#include <string>
#include <vector>

namespace ubuntu {
namespace crypto {

/**
 * @brief Base58 encoding/decoding utilities
 *
 * Base58 is used in Bitcoin-style addresses to avoid ambiguous characters
 * (0, O, I, l) and provide a compact representation.
 */
namespace Base58 {

/**
 * @brief Encode data to Base58 string
 *
 * @param data Input data to encode
 * @return Base58-encoded string
 */
std::string encode(std::span<const uint8_t> data);

/**
 * @brief Decode Base58 string to bytes
 *
 * @param encoded Base58-encoded string
 * @return Decoded bytes, or empty vector if invalid
 */
std::vector<uint8_t> decode(const std::string& encoded);

/**
 * @brief Base58Check encoding (with checksum)
 *
 * Adds a 4-byte checksum (first 4 bytes of SHA256d) to the data
 * before encoding. Used for Bitcoin addresses.
 *
 * @param data Input data to encode
 * @return Base58Check-encoded string
 */
std::string encodeCheck(std::span<const uint8_t> data);

/**
 * @brief Base58Check decoding (with checksum verification)
 *
 * @param encoded Base58Check-encoded string
 * @return Decoded bytes (without checksum), or empty if invalid/checksum mismatch
 */
std::vector<uint8_t> decodeCheck(const std::string& encoded);

}  // namespace Base58

/**
 * @brief Bech32 encoding/decoding utilities (for SegWit-style addresses)
 *
 * Bech32 is a newer address format with better error detection properties.
 */
namespace Bech32 {

/**
 * @brief Encode data to Bech32 string
 *
 * @param hrp Human-readable part (e.g., "u" for mainnet, "tu" for testnet)
 * @param data Data to encode (witness program)
 * @return Bech32-encoded string (e.g., "u1...")
 */
std::string encode(const std::string& hrp, std::span<const uint8_t> data);

/**
 * @brief Decode Bech32 string
 *
 * @param bech32 Bech32-encoded string
 * @return Pair of (HRP, decoded data), or empty data if invalid
 */
std::pair<std::string, std::vector<uint8_t>> decode(const std::string& bech32);

}  // namespace Bech32

}  // namespace crypto
}  // namespace ubuntu
