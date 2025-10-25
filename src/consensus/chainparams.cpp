#include "ubuntu/consensus/chainparams.h"

namespace ubuntu {
namespace consensus {

ChainParams ChainParamsFactory::mainnet() {
    ChainParams params;

    // Network identification
    params.networkType = NetworkType::MAINNET;
    params.networkName = "main";
    params.magicBytes = 0xD9B4BEF9;  // Network message magic

    // Genesis block
    params.genesisTimestamp = 1704067200;  // Jan 1, 2024 00:00:00 UTC
    params.genesisDifficulty = 0x1d00ffff;
    params.genesisNonce = 0;
    // genesisHash will be set after mining genesis block

    // Proof of Work
    params.powTargetTimespan = Constants::POW_TARGET_TIMESPAN;  // 2 weeks
    params.powTargetSpacing = Constants::POW_TARGET_SPACING;    // 60 seconds
    params.difficultyAdjustmentInterval = Constants::DIFFICULTY_ADJUSTMENT_INTERVAL;  // 2016 blocks
    params.allowMinDifficultyBlocks = false;
    params.noRetargeting = false;

    // Block validation
    params.maxBlockSize = Constants::MAX_BLOCK_SIZE;  // 1 MB
    params.coinbaseMaturity = Constants::COINBASE_MATURITY;  // 100 blocks
    params.maxMoneySupply = Constants::MAX_MONEY;  // 21 million UBU

    // Mining rewards
    params.initialBlockReward = 50 * Constants::COIN;  // 50 UBU
    params.halvingInterval = 4 * 365 * 24 * 60;  // 4 years in 1-minute blocks

    // Network
    params.defaultPort = 8333;
    params.rpcPort = 8332;

    // Address prefixes
    params.base58PubkeyPrefix = 0x3C;  // Mainnet P2PKH starts with 'U'
    params.base58ScriptPrefix = 0x05;  // Mainnet P2SH
    params.bech32Prefix = "ubu";       // Mainnet Bech32

    // Checkpoints (will be added as network matures)
    params.checkpoints = {
        // {height, blockHash}
        // Example: {0, genesisHash}
    };

    return params;
}

ChainParams ChainParamsFactory::testnet() {
    ChainParams params;

    // Network identification
    params.networkType = NetworkType::TESTNET;
    params.networkName = "test";
    params.magicBytes = 0x0B110907;  // Different magic for testnet

    // Genesis block
    params.genesisTimestamp = 1704067200;  // Same as mainnet for simplicity
    params.genesisDifficulty = 0x1d00ffff;
    params.genesisNonce = 0;

    // Proof of Work
    params.powTargetTimespan = Constants::POW_TARGET_TIMESPAN;
    params.powTargetSpacing = Constants::POW_TARGET_SPACING;
    params.difficultyAdjustmentInterval = Constants::DIFFICULTY_ADJUSTMENT_INTERVAL;
    params.allowMinDifficultyBlocks = true;  // Allow min difficulty on testnet
    params.noRetargeting = false;

    // Block validation
    params.maxBlockSize = Constants::MAX_BLOCK_SIZE;
    params.coinbaseMaturity = Constants::COINBASE_MATURITY;
    params.maxMoneySupply = Constants::MAX_MONEY;

    // Mining rewards (same as mainnet)
    params.initialBlockReward = 50 * Constants::COIN;
    params.halvingInterval = 4 * 365 * 24 * 60;

    // Network
    params.defaultPort = 18333;
    params.rpcPort = 18332;

    // Address prefixes
    params.base58PubkeyPrefix = 0x6F;  // Testnet P2PKH starts with 'm' or 'n'
    params.base58ScriptPrefix = 0xC4;  // Testnet P2SH
    params.bech32Prefix = "tubu";      // Testnet Bech32

    // No checkpoints on testnet (reset frequently)
    params.checkpoints = {};

    return params;
}

ChainParams ChainParamsFactory::regtest() {
    ChainParams params;

    // Network identification
    params.networkType = NetworkType::REGTEST;
    params.networkName = "regtest";
    params.magicBytes = 0xFABFB5DA;  // Regtest magic

    // Genesis block
    params.genesisTimestamp = 1704067200;
    params.genesisDifficulty = 0x207fffff;  // Very low difficulty
    params.genesisNonce = 0;

    // Proof of Work
    params.powTargetTimespan = Constants::POW_TARGET_TIMESPAN;
    params.powTargetSpacing = Constants::POW_TARGET_SPACING;
    params.difficultyAdjustmentInterval = Constants::DIFFICULTY_ADJUSTMENT_INTERVAL;
    params.allowMinDifficultyBlocks = true;
    params.noRetargeting = true;  // No difficulty retargeting on regtest

    // Block validation
    params.maxBlockSize = Constants::MAX_BLOCK_SIZE;
    params.coinbaseMaturity = 100;  // Can be adjusted for testing
    params.maxMoneySupply = Constants::MAX_MONEY;

    // Mining rewards
    params.initialBlockReward = 50 * Constants::COIN;
    params.halvingInterval = 150;  // Fast halving for testing

    // Network
    params.defaultPort = 18444;
    params.rpcPort = 18443;

    // Address prefixes (same as testnet)
    params.base58PubkeyPrefix = 0x6F;
    params.base58ScriptPrefix = 0xC4;
    params.bech32Prefix = "rubu";  // Regtest Bech32

    // No checkpoints on regtest
    params.checkpoints = {};

    return params;
}

ChainParams ChainParamsFactory::forNetwork(NetworkType network) {
    switch (network) {
        case NetworkType::MAINNET:
            return mainnet();
        case NetworkType::TESTNET:
            return testnet();
        case NetworkType::REGTEST:
            return regtest();
        default:
            return mainnet();
    }
}

}  // namespace consensus
}  // namespace ubuntu
