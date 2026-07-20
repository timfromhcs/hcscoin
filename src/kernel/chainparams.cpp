// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <crypto/hex_base.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/verify_flags.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/log.h>
#include <util/strencodings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <span>
#include <utility>

using namespace util::hex_literals;

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the HCScoin genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * HCScoin mainnet genesis (mined with tools/genesis/genesis_miner.c and
 * independently verified with tools/genesis/verify_genesis.py):
 *
 * CBlock(hash=0000027ab22fdc, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=d0ad4e, nTime=1784505600, nBits=1e0fffff, nNonce=2862199, vtx=1)
 *   CTransaction(hash=d0ad4e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01541e484353636f696e2047656e65736973202d203230204a756c792032303236)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: d0ad4e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // HCScoin: new genesis timestamp. No Bitcoin history is carried over.
    const char* pszTimestamp = "HCScoin Genesis - 20 July 2026";
    const CScript genesisOutputScript = CScript() << "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"_hex << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::ApplyDeploymentOptions(const DeploymentOptions& opts)
{
    for (const auto& [dep, height] : opts.activation_heights) {
        switch (dep) {
        case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
            consensus.SegwitHeight = int{height};
            break;
        case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
            consensus.BIP34Height = int{height};
            break;
        case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
            consensus.BIP66Height = int{height};
            break;
        case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
            consensus.BIP65Height = int{height};
            break;
        case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
            consensus.CSVHeight = int{height};
            break;
        }
    }

    for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
        consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
        consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
        consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
    }
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams(const MainNetOptions& opts) {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // HCScoin: 840'000-block halving interval (see doc/QUANTUM.md economics).
        consensus.nSubsidyHalvingInterval = 840000;
        // HCScoin: fresh chain - no Bitcoin script flag exceptions are needed.
        // HCScoin: all softfork deployments active from genesis (height 1,
        // matching the upstream convention for new chains; BIP34Hash is left
        // null so BIP30 enforcement stays fully active).
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        // HCScoin: powLimit per project specification (00000fff...).
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        // HCScoin: quantum PoW - 27-qubit Panta-Sim proof, difficulty target
        // 2^252 (~16 simulations per block on average).
        consensus.quantumPowLimit = uint256{"0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nQuantumQubits = 27;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        ApplyDeploymentOptions(opts.dep_opts);

        // HCScoin: brand new chain - no minimum work, no assume-valid block.
        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        /**
         * HCScoin message start string (magic bytes): 0xF2E5D4C3.
         * Unique to HCScoin; unlikely to occur in normal data and not valid
         * as UTF-8.
         */
        pchMessageStart[0] = 0xF2;
        pchMessageStart[1] = 0xE5;
        pchMessageStart[2] = 0xD4;
        pchMessageStart[3] = 0xC3;
        nDefaultPort = 28333; // HCScoin mainnet P2P port (was 8333)
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 2;
        m_assumed_chain_state_size = 1;

        // HCScoin genesis: 2026-07-20 00:00:00 UTC, mined at nBits 0x1e0fffff
        // (extended header with quantum fields; see tools/genesis/).
        genesis = CreateGenesisBlock(1784505600, 194955, 0x1e0fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000001ba4186098ba71b34022db82ece3e8ec812a1bf2ccfd08a3ad2f5ad6a77"});
        assert(genesis.hashMerkleRoot == uint256{"d0ad4e8d4987ab763bc753a6b3c00cc820b86fc5fca525dfc6a273f5930ee2e3"});

        // HCScoin: Bitcoin DNS seeds removed; HCScoin seed infrastructure.
        vSeeds.emplace_back("dnsseed.hcscoin.net.");  // HCScoin primary DNS seed
        vSeeds.emplace_back("seed.hcscoin.org.");     // HCScoin secondary DNS seed

        // HCScoin address prefixes:
        //   PUBKEY_ADDRESS 40  -> base58 addresses start with 'H'
        //   SCRIPT_ADDRESS 100 -> base58 addresses start with 'h'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,40);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,100);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x02, 0x8A, 0x4C, 0x51}; // hpub
        base58Prefixes[EXT_SECRET_KEY] = {0x02, 0x8A, 0x4B, 0x99}; // hprv

        // HCScoin Bech32/Bech32m HRP: hcs1... addresses
        bech32_hrp = "hcs";

        // HCScoin: no fixed seeds yet (fresh network).
        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // HCScoin: no assumeutxo snapshots exist yet for the new chain.
        m_assumeutxo_data = {};

        // HCScoin: chain starts at genesis (1 transaction).
        chainTxData = ChainTxData{
            .nTime    = 1784505600,
            .tx_count = 1,
            .dTxRate  = 0,
        };

        // Generated by headerssync-params.py on 2026-02-25.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 641,
            .redownload_buffer_size = 15218, // 15218/641 = ~23.7 commitments
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams(const TestNetOptions& opts) {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 840000; // HCScoin
        // HCScoin: fresh chain - no script flag exceptions, all deployments
        // active from genesis.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"}; // HCScoin
        // HCScoin: quantum PoW - 27 qubits, easy target for the test network.
        consensus.quantumPowLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nQuantumQubits = 27;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        ApplyDeploymentOptions(opts.dep_opts);

        // HCScoin: fresh testnet chain.
        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // HCScoin testnet3 magic bytes (mainnet magic + 0x01010101).
        pchMessageStart[0] = 0xF3;
        pchMessageStart[1] = 0xE6;
        pchMessageStart[2] = 0xD5;
        pchMessageStart[3] = 0xC4;
        nDefaultPort = 28334; // HCScoin testnet3 P2P port (was 18333)
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        // HCScoin testnet3 genesis: 2026-07-20 00:05:00 UTC.
        genesis = CreateGenesisBlock(1784505900, 665599, 0x1e0fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"0000077168046ce53f36b56fbc66a23821ae24de408a716cbc35d643bc1d1e59"});
        assert(genesis.hashMerkleRoot == uint256{"d0ad4e8d4987ab763bc753a6b3c00cc820b86fc5fca525dfc6a273f5930ee2e3"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // HCScoin testnet seeds
        vSeeds.emplace_back("testnet-seed.hcscoin.net.");
        vSeeds.emplace_back("testnet-seed.hcscoin.org.");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);  // 'T'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // HCScoin testnet Bech32 HRP: thcs1...
        bech32_hrp = "thcs";

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // HCScoin: no assumeutxo snapshots for the new testnet.
        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime    = 1784505900,
            .tx_count = 1,
            .dTxRate  = 0,
        };

        // Generated by headerssync-params.py on 2026-02-25.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 673,
            .redownload_buffer_size = 14460, // 14460/673 = ~21.5 commitments
        };
    }
};

/**
 * Testnet (v4): public test network which is reset from time to time.
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params(const TestNetOptions& opts) {
        m_chain_type = ChainType::TESTNET4;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 840000; // HCScoin
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"}; // HCScoin
        // HCScoin: quantum PoW - 27 qubits, easy target for testnet4.
        consensus.quantumPowLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nQuantumQubits = 27;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        ApplyDeploymentOptions(opts.dep_opts);

        // HCScoin: fresh testnet4 chain.
        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // HCScoin testnet4 magic bytes.
        pchMessageStart[0] = 0xF4;
        pchMessageStart[1] = 0xE7;
        pchMessageStart[2] = 0xD6;
        pchMessageStart[3] = 0xC5;
        nDefaultPort = 28338; // HCScoin testnet4 P2P port (was 48333)
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        // HCScoin testnet4 genesis: 2026-07-20 00:20:00 UTC.
        genesis = CreateGenesisBlock(1784506800, 300699, 0x1e0fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000008038118a320d4c95a6b28f1606a489b4eb544c530a55b2926dd8979b6a3"});
        assert(genesis.hashMerkleRoot == uint256{"d0ad4e8d4987ab763bc753a6b3c00cc820b86fc5fca525dfc6a273f5930ee2e3"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // HCScoin testnet4 seeds
        vSeeds.emplace_back("testnet4-seed.hcscoin.net.");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);  // 'T'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // HCScoin testnet4 Bech32 HRP: thcs1...
        bech32_hrp = "thcs";

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // HCScoin: no assumeutxo snapshots for the new testnet4.
        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime    = 1784506800,
            .tx_count = 1,
            .dTxRate  = 0,
        };

        // Generated by headerssync-params.py on 2026-02-25.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 606,
            .redownload_buffer_size = 16092, // 16092/606 = ~26.6 commitments
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vFixedSeeds.clear();
        vSeeds.clear();

        if (!options.challenge) {
            bin = "512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae"_hex_v_u8;
            // HCScoin: fresh signet - no fixed seeds, no chain history.
            vFixedSeeds.clear();
            vSeeds.emplace_back("signet-seed.hcscoin.net.");

            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 1;
            chainTxData = ChainTxData{
                .nTime    = 1784506500,
                .tx_count = 1,
                .dTxRate  = 0,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogInfo("Signet with challenge %s", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 840000; // HCScoin
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000377ae000000000000000000000000000000000000000000000000000000"};
        // HCScoin: quantum PoW - 27 qubits, easy target for signet.
        consensus.quantumPowLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nQuantumQubits = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        ApplyDeploymentOptions(options.dep_opts);

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 28340; // HCScoin signet P2P port (was 38333)
        nPruneAfterHeight = 1000;

        // HCScoin signet genesis: 2026-07-20 00:15:00 UTC.
        genesis = CreateGenesisBlock(1784506500, 12314611, 0x1e0377ae, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000001af6eeb35a97c548aedf175b6a87f98b1056fa359db0d8b6b84cfe34f2f"});
        assert(genesis.hashMerkleRoot == uint256{"d0ad4e8d4987ab763bc753a6b3c00cc820b86fc5fca525dfc6a273f5930ee2e3"});

        // HCScoin: no assumeutxo snapshots for the new signet.
        m_assumeutxo_data = {};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // HCScoin signet Bech32 HRP: thcs1...
        bech32_hrp = "thcs";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // Generated by headerssync-params.py on 2026-02-25.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 620,
            .redownload_buffer_size = 15724, // 15724/620 = ~25.4 commitments
        };
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        // HCScoin: quantum PoW on regtest uses a small circuit so unit and
        // functional tests run fast; target is trivial (2^255).
        consensus.quantumPowLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nQuantumQubits = 12;
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // HCScoin regtest magic bytes.
        pchMessageStart[0] = 0xF5;
        pchMessageStart[1] = 0xE8;
        pchMessageStart[2] = 0xD7;
        pchMessageStart[3] = 0xC6;
        nDefaultPort = 28335; // HCScoin regtest P2P port (was 18444)
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        ApplyDeploymentOptions(opts.dep_opts);

        // HCScoin regtest genesis: 2026-07-20 00:10:00 UTC.
        genesis = CreateGenesisBlock(1784506200, 2, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"025cd4a4693b3e329b10e052cd502fa7a896ef821fe075ee52711776273f83dd"});
        assert(genesis.hashMerkleRoot == uint256{"d0ad4e8d4987ab763bc753a6b3c00cc820b86fc5fca525dfc6a273f5930ee2e3"});

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = {
            {   // For use by unit tests
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256{"86e9a1205b418b16dde3a18a78c730e30137e28466bda5dbf6b33ab8fc05447c"}},
                .m_chain_tx_count = 111,
                .blockhash = uint256{"135eec25a6fb277884e5824e7aa7d052c4868161c99a5122170b5266f86c273d"},
            },
            {
                // For use by fuzz target src/test/fuzz/utxo_snapshot.cpp
                .height = 200,
                .hash_serialized = AssumeutxoHash{uint256{"17dcc016d188d16068907cdeb38b75691a118d43053b8cd6a25969419381d13a"}},
                .m_chain_tx_count = 201,
                .blockhash = uint256{"385901ccbd69dff6bbd00065d01fb8a9e464dede7cfe0372443884f9b1dcf6b9"},
            },
            {
                // For use by test/functional/feature_assumeutxo.py and test/functional/tool_bitcoin_chainstate.py
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256{"106b2c56233e378a824cf0d5ff2be42ed32c72f1605c9be288d00942908a40ac"}},
                .m_chain_tx_count = 334,
                .blockhash = uint256{"0c552ced4721c249a389eb9b08cb8da261cd46f0e7b5f9d064d48f3113406853"},
            },
        };

        chainTxData = ChainTxData{
            .nTime = 0,
            .tx_count = 0,
            .dTxRate = 0.001, // Set a non-zero rate to make it testable
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);  // HCScoin: 'T'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "hcsrt"; // HCScoin regtest

        // Copied from Testnet4.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 275,
            .redownload_buffer_size = 7017, // 7017/275 = ~25.5 commitments
        };
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main(const MainNetOptions& options)
{
    return std::make_unique<const CMainParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::TestNet(const TestNetOptions& options)
{
    return std::make_unique<const CTestNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::TestNet4(const TestNetOptions& options)
{
    return std::make_unique<const CTestNet4Params>(options);
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest()->MessageStart();
    const auto signet_msg = CChainParams::SigNet()->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
