#include "ubuntu/crypto/base58.h"
#include "ubuntu/crypto/hash.h"

#include <algorithm>
#include <stdexcept>

namespace ubuntu {
namespace crypto {

// Base58 alphabet (Bitcoin-style, excludes 0, O, I, l)
static const char* BASE58_ALPHABET =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// ============================================================================
// Base58 Implementation
// ============================================================================

namespace Base58 {

std::string encode(std::span<const uint8_t> data) {
    if (data.empty()) {
        return "";
    }

    // Count leading zeros
    size_t leadingZeros = 0;
    while (leadingZeros < data.size() && data[leadingZeros] == 0) {
        ++leadingZeros;
    }

    // Convert to base58
    std::vector<uint8_t> temp(data.begin(), data.end());
    std::string result;

    while (!temp.empty() && !(temp.size() == 1 && temp[0] == 0)) {
        // Divide by 58 and get remainder
        int carry = 0;
        std::vector<uint8_t> next;

        for (size_t i = 0; i < temp.size(); ++i) {
            int value = carry * 256 + temp[i];
            if (value >= 58) {
                next.push_back(value / 58);
                carry = value % 58;
            } else if (!next.empty()) {
                next.push_back(0);
                carry = value;
            } else {
                carry = value;
            }
        }

        result.push_back(BASE58_ALPHABET[carry]);
        temp = std::move(next);
    }

    // Add leading '1' for each leading zero byte
    for (size_t i = 0; i < leadingZeros; ++i) {
        result.push_back('1');
    }

    // Reverse the result
    std::reverse(result.begin(), result.end());

    return result;
}

std::vector<uint8_t> decode(const std::string& encoded) {
    if (encoded.empty()) {
        return {};
    }

    // Count leading '1's
    size_t leadingOnes = 0;
    while (leadingOnes < encoded.length() && encoded[leadingOnes] == '1') {
        ++leadingOnes;
    }

    // Decode from base58
    std::vector<uint8_t> result;

    for (char c : encoded) {
        const char* pos = std::strchr(BASE58_ALPHABET, c);
        if (!pos) {
            // Invalid character
            return {};
        }

        int digit = pos - BASE58_ALPHABET;

        // Multiply result by 58 and add digit
        int carry = digit;
        for (size_t i = 0; i < result.size(); ++i) {
            int value = result[i] * 58 + carry;
            result[i] = value & 0xFF;
            carry = value >> 8;
        }

        while (carry > 0) {
            result.push_back(carry & 0xFF);
            carry >>= 8;
        }
    }

    // Add leading zero bytes
    for (size_t i = 0; i < leadingOnes; ++i) {
        result.push_back(0);
    }

    // Reverse the result
    std::reverse(result.begin(), result.end());

    return result;
}

std::string encodeCheck(std::span<const uint8_t> data) {
    // Compute checksum (first 4 bytes of SHA256d)
    auto hash = sha256d(data);

    // Append checksum to data
    std::vector<uint8_t> dataWithChecksum(data.begin(), data.end());
    dataWithChecksum.insert(dataWithChecksum.end(),
                            hash.begin(),
                            hash.begin() + 4);

    // Encode with Base58
    return encode(std::span<const uint8_t>(dataWithChecksum.data(),
                                            dataWithChecksum.size()));
}

std::vector<uint8_t> decodeCheck(const std::string& encoded) {
    auto decoded = decode(encoded);

    if (decoded.size() < 4) {
        // Too short to contain checksum
        return {};
    }

    // Split data and checksum
    std::vector<uint8_t> data(decoded.begin(), decoded.end() - 4);
    std::vector<uint8_t> checksum(decoded.end() - 4, decoded.end());

    // Verify checksum
    auto hash = sha256d(std::span<const uint8_t>(data.data(), data.size()));

    if (!std::equal(checksum.begin(), checksum.end(), hash.begin())) {
        // Checksum mismatch
        return {};
    }

    return data;
}

}  // namespace Base58

// ============================================================================
// Bech32 Implementation
// ============================================================================

namespace Bech32 {

// Bech32 character set
static const char* CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

// Generator polynomial for Bech32 checksum
static const uint32_t GENERATOR[] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa,
                                      0x3d4233dd, 0x2a1462b3};

static uint32_t polymod(const std::vector<uint8_t>& values) {
    uint32_t chk = 1;
    for (uint8_t value : values) {
        uint32_t top = chk >> 25;
        chk = (chk & 0x1ffffff) << 5 ^ value;
        for (int i = 0; i < 5; ++i) {
            if ((top >> i) & 1) {
                chk ^= GENERATOR[i];
            }
        }
    }
    return chk;
}

static std::vector<uint8_t> hrpExpand(const std::string& hrp) {
    std::vector<uint8_t> result;
    for (char c : hrp) {
        result.push_back(c >> 5);
    }
    result.push_back(0);
    for (char c : hrp) {
        result.push_back(c & 31);
    }
    return result;
}

static std::vector<uint8_t> createChecksum(const std::string& hrp,
                                            std::span<const uint8_t> data) {
    auto values = hrpExpand(hrp);
    values.insert(values.end(), data.begin(), data.end());
    values.insert(values.end(), 6, 0);

    uint32_t mod = polymod(values) ^ 1;

    std::vector<uint8_t> checksum(6);
    for (size_t i = 0; i < 6; ++i) {
        checksum[i] = (mod >> (5 * (5 - i))) & 31;
    }

    return checksum;
}

std::string encode(const std::string& hrp, std::span<const uint8_t> data) {
    // Convert 8-bit data to 5-bit groups
    std::vector<uint8_t> data5bit;
    int acc = 0;
    int bits = 0;

    for (uint8_t byte : data) {
        acc = (acc << 8) | byte;
        bits += 8;

        while (bits >= 5) {
            bits -= 5;
            data5bit.push_back((acc >> bits) & 31);
        }
    }

    if (bits > 0) {
        data5bit.push_back((acc << (5 - bits)) & 31);
    }

    // Create checksum
    auto checksum = createChecksum(hrp, std::span<const uint8_t>(data5bit.data(),
                                                                  data5bit.size()));

    // Build result
    std::string result = hrp + "1";

    for (uint8_t value : data5bit) {
        result += CHARSET[value];
    }

    for (uint8_t value : checksum) {
        result += CHARSET[value];
    }

    return result;
}

std::pair<std::string, std::vector<uint8_t>> decode(const std::string& bech32) {
    // Find separator
    size_t pos = bech32.rfind('1');
    if (pos == std::string::npos || pos == 0 || pos + 7 > bech32.length()) {
        return {"", {}};
    }

    std::string hrp = bech32.substr(0, pos);
    std::string data = bech32.substr(pos + 1);

    // Convert characters to values
    std::vector<uint8_t> values;
    for (char c : data) {
        const char* p = std::strchr(CHARSET, c);
        if (!p) {
            return {"", {}};
        }
        values.push_back(p - CHARSET);
    }

    // Verify checksum
    auto hrpExpanded = hrpExpand(hrp);
    hrpExpanded.insert(hrpExpanded.end(), values.begin(), values.end());

    if (polymod(hrpExpanded) != 1) {
        return {"", {}};  // Checksum failed
    }

    // Remove checksum (last 6 characters)
    values.resize(values.size() - 6);

    // Convert 5-bit to 8-bit
    std::vector<uint8_t> result;
    int acc = 0;
    int bits = 0;

    for (uint8_t value : values) {
        acc = (acc << 5) | value;
        bits += 5;

        if (bits >= 8) {
            bits -= 8;
            result.push_back((acc >> bits) & 255);
        }
    }

    if (bits >= 5 || ((acc << (8 - bits)) & 255)) {
        return {"", {}};  // Invalid padding
    }

    return {hrp, result};
}

}  // namespace Bech32

}  // namespace crypto
}  // namespace ubuntu
