// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Airdrop contract implementation.

#include <consensus/airdrop.h>

#include <logging.h>
#include <random.h>

void CAirdropContract::AddFees(CAmount fees) { m_balance += fees / 2; }
void CAirdropContract::AddSubsidyShare(CAmount subsidy) { m_balance += subsidy / 2; }

bool CAirdropContract::DrawLottery(int64_t nBlockTime, std::vector<std::string>& winnersOut)
{
    // Determine month from block time: months since Unix epoch
    struct tm* ptm = gmtime(&nBlockTime);
    if (!ptm) return false;
    int currentMonth = (ptm->tm_year - 70) * 12 + ptm->tm_mon; // months since epoch
    if (currentMonth <= m_monthCounter) return false; // already drawn this month

    LogInfo("Airdrop lottery triggered at block time %lld (month %d -> %d)",
            nBlockTime, m_monthCounter, currentMonth);
    m_monthCounter = currentMonth;
    m_lastDrawTime = nBlockTime;

    if (m_balance < COIN) {
        LogInfo("Airdrop balance %d HCS too small for distribution.", m_balance / COIN);
        winnersOut.clear();
        return true; // draw happened, but no meaningful winners
    }

    Distribute(winnersOut);
    return true;
}

void CAirdropContract::Distribute(const std::vector<std::string>& winners)
{
    if (winners.empty() || m_balance <= 0) return;
    CAmount share = m_balance / winners.size();
    for (const auto& w : winners) {
        LogInfo("Airdrop: %d HCS awarded to %s", share / COIN, w);
    }
    m_balance = 0; // consumed
}

std::vector<uint256> CAirdropContract::SelectWinners(const uint256& blockHash) const
{
    // In production this would sample from the active UTXO set using
    // deterministic selection anchored at the block hash. For the
    // core implementation, we provide placeholder winners drawn from
    // a hardcoded test set (to be replaced by the wallet/UTXO layer).
    // See doc/DEVELOPER_NOTES.md on integrating with CCoinsView.
    std::vector<uint256> winners;
    winners.reserve(AIRDROP_NUM_WINNERS);
    FastRandomContext rng{blockHash};
    for (int i = 0; i < AIRDROP_NUM_WINNERS; ++i) {
        uint256 addr;
        rng.fillrand(addr);
        winners.push_back(addr);
    }
    return winners;
}
