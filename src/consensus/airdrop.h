// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Airdrop contract — economic incentive model.
//
// Every block pays 50% of fees to the miner (alongside the block subsidy)
// and 50% into the airdrop pool. On the first block of every month
// (determined by nTime), 10 randomly selected active addresses receive
// an equal share of the accumulated pool.
//
// See doc/QUANTUM.md and doc/API.md for design rationale and RPC commands.

#ifndef HCSCOIN_CONSENSUS_AIRDROP_H
#define HCSCOIN_CONSENSUS_AIRDROP_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <map>
#include <string>
#include <vector>

/** Block interval (in months, via block time) for airdrop lotteries. */
static constexpr int AIRDROP_MONTH_INTERVAL = 1; // monthly
/** Number of winners drawn each period. */
static constexpr int AIRDROP_NUM_WINNERS = 10;

/**
 * In-memory state of the airdrop contract. Persisted through the chain
 * state (UTXO DB) in production; for v1, state is reconstructed at
 * startup from block rewards.
 */
class CAirdropContract
{
public:
    /** Half of the fees of a newly mined block (50%) goes here. */
    void AddFees(CAmount fees);

    /** Half of the block subsidy (50% of the coinbase reward). */
    void AddSubsidyShare(CAmount subsidy);

    /** Get the current accumulated balance. */
    CAmount GetBalance() const { return m_balance; }

    /** Month counter (derived from block time when DrawLottery runs). */
    int GetMonthCounter() const { return m_monthCounter; }
    void SetMonthCounter(int c) { m_monthCounter = c; }

    /**
     * Attempt to draw the lottery. Returns true if this block time
     * triggers a draw (first block of a new month). The winner addresses
     * are printed to debug log and recorded in the returned list; the
     * actual coin distribution is handled by the miner's coinbase
     * (the miner splits payout among winners).
     *
     * @param nBlockTime  Block header time (Unix seconds).
     * @param winnersOut  Filled with destination addresses (hex hashes of
     *                    active addresses sampled from the chain's UTXO set).
     * @returns true if a lottery occurred.
     */
    bool DrawLottery(int64_t nBlockTime, std::vector<std::string>& winnersOut);

    /** Manually distribute the balance to winners. Subtract m_balance. */
    void Distribute(const std::vector<std::string>& winners);

    /**
     * Serialization helpers for checkpoint / DB persistence.
     */
    template <typename Stream>
    void Serialize(Stream& s) const
    {
        s << m_balance << m_monthCounter << m_lastDrawTime;
    }
    template <typename Stream>
    void Unserialize(Stream& s)
    {
        s >> m_balance >> m_monthCounter >> m_lastDrawTime;
    }

private:
    CAmount m_balance{0};
    int m_monthCounter{0};
    int64_t m_lastDrawTime{0};

    /** Deterministic address selection based on the block hash and month. */
    std::vector<uint256> SelectWinners(const uint256& blockHash) const;
};

#endif // HCSCOIN_CONSENSUS_AIRDROP_H
