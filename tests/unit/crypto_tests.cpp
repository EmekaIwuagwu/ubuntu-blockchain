#include "ubuntu/crypto/hash.h"
#include "ubuntu/crypto/keys.h"
#include "ubuntu/crypto/signatures.h"
#include "ubuntu/crypto/base58.h"

#include <gtest/gtest.h>

using namespace ubuntu::crypto;

// ===== Hash Tests =====

TEST(HashTest, SHA256Basic) {
    std::string input = "Hello, Ubuntu Blockchain!";
    auto hash = sha256(input);
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Hash256::zero());
}

TEST(HashTest, SHA256d) {
    std::string input = "test";
    auto hash1 = sha256(input);
    auto hash2 = sha256(hash1.toVector());
    auto hashd = sha256d(input);
    EXPECT_EQ(hash2, hashd);
}

TEST(HashTest, Hash160) {
    std::string input = "Ubuntu";
    auto hash = hash160(input);
    EXPECT_EQ(hash.size(), 20);
}

TEST(HashTest, HexConversion) {
    auto hash = Hash256::zero();
    auto hex = hash.toHex();
    auto restored = Hash256::fromHex(hex);
    EXPECT_EQ(hash, restored);
}

// ===== Key Tests =====

TEST(KeyTest, KeyGeneration) {
    auto privKey = PrivateKey::generate();
    EXPECT_TRUE(privKey.isValid());
}

TEST(KeyTest, PublicKeyDerivation) {
    auto privKey = PrivateKey::generate();
    auto pubKey = PublicKey::fromPrivateKey(privKey, true);
    EXPECT_TRUE(pubKey.isValid());
    EXPECT_TRUE(pubKey.isCompressed());
}

TEST(KeyTest, KeyPairGeneration) {
    auto keyPair = KeyPair::generate();
    EXPECT_TRUE(keyPair.privateKey.isValid());
    EXPECT_TRUE(keyPair.publicKey.isValid());
}

// ===== Signature Tests =====

TEST(SignatureTest, SignAndVerify) {
    auto keyPair = KeyPair::generate();
    auto message = sha256("Test message");
    
    auto signature = Signer::sign(message, keyPair.privateKey);
    EXPECT_TRUE(signature.isValid());
    
    bool verified = Signer::verify(message, signature, keyPair.publicKey);
    EXPECT_TRUE(verified);
}

TEST(SignatureTest, VerifyFailsWithWrongKey) {
    auto keyPair1 = KeyPair::generate();
    auto keyPair2 = KeyPair::generate();
    auto message = sha256("Test message");
    
    auto signature = Signer::sign(message, keyPair1.privateKey);
    bool verified = Signer::verify(message, signature, keyPair2.publicKey);
    EXPECT_FALSE(verified);
}

TEST(SignatureTest, VerifyFailsWithWrongMessage) {
    auto keyPair = KeyPair::generate();
    auto message1 = sha256("Message 1");
    auto message2 = sha256("Message 2");
    
    auto signature = Signer::sign(message1, keyPair.privateKey);
    bool verified = Signer::verify(message2, signature, keyPair.publicKey);
    EXPECT_FALSE(verified);
}

// ===== Base58 Tests =====

TEST(Base58Test, EncodeDecodeBasic) {
    std::string input = "Hello World";
    std::vector<uint8_t> data(input.begin(), input.end());
    
    auto encoded = Base58::encode(data);
    EXPECT_FALSE(encoded.empty());
    
    auto decoded = Base58::decode(encoded);
    EXPECT_EQ(data, decoded);
}

TEST(Base58Test, EncodeCheckWithChecksum) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0x04};
    auto encoded = Base58::encodeCheck(data);
    EXPECT_FALSE(encoded.empty());
    
    auto decoded = Base58::decodeCheck(encoded);
    EXPECT_EQ(data, decoded);
}

TEST(Base58Test, DecodeCheckFailsOnBadChecksum) {
    auto encoded = Base58::encodeCheck({0x01, 0x02, 0x03});
    // Modify last character to corrupt checksum
    if (!encoded.empty()) {
        encoded[encoded.length() - 1] = '1';
    }
    auto decoded = Base58::decodeCheck(encoded);
    EXPECT_TRUE(decoded.empty());
}

// ===== Bech32 Tests =====

TEST(Bech32Test, EncodeDecode) {
    std::string hrp = "ubu";
    std::vector<uint8_t> data = {0x00, 0x14, 0x75, 0x1e, 0x76};
    
    auto encoded = Bech32::encode(hrp, data);
    EXPECT_FALSE(encoded.empty());
    EXPECT_EQ(encoded.substr(0, 4), "ubu1");
    
    auto [decodedHrp, decodedData] = Bech32::decode(encoded);
    EXPECT_EQ(hrp, decodedHrp);
    EXPECT_EQ(data, decodedData);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
