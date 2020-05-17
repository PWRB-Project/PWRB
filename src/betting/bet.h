// Copyright (c) 2018 The Wagerr developers
// Copyright (c) 2020 The powerbalt developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POWERALT_BET_H
#define POWERALT_BET_H

#include "util.h"
#include "chainparams.h"

#include <boost/filesystem/path.hpp>
#include <map>
#include <iomanip>
#include <univalue/include/univalue.h>

// The supported betting TX types.
typedef enum BetTxTypes{
    plBetTxType          = 0x01,  // Peerless Bet transaction type identifier.
} BetTxTypes;

// Class derived from CTxOut
// nBetValue is NOT serialized, nor is it included in the hash.
class CBetOut : public CTxOut {
    public:

    CAmount nBetValue;
    uint32_t nDate;
    uint32_t nNumber1;
    uint32_t nNumber2;
    uint32_t nNumber3;

    CBetOut() : CTxOut() {
        SetNull();
    }

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn) : CTxOut(nValueIn, scriptPubKeyIn), nBetValue(0), nDate(0), nNumber1(0), nNumber2(0), nNumber3(0) {};

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn) :
            CTxOut(nValueIn, scriptPubKeyIn), nBetValue(nBetValueIn), nDate(0), nNumber1(0), nNumber2(0), nNumber3(0) {};

    CBetOut(const CAmount& nValueIn, CScript scriptPubKeyIn, const CAmount& nBetValueIn, uint32_t nDateIn) :
            CTxOut(nValueIn, scriptPubKeyIn), nBetValue(nBetValueIn), nDate(nDateIn), nNumber1(0), nNumber2(0), nNumber3(0) {};

    void SetNull() {
        CTxOut::SetNull();
        nBetValue = -1;
        nDate = -1;
        nNumber1 = -1;
        nNumber2 = -1;
        nNumber3 = -1;
    }

    void SetEmpty() {
        CTxOut::SetEmpty();
        nBetValue = 0;
        nDate = 0;
        nNumber1 = 0;
        nNumber2 = 0;
        nNumber3 = 0;
    }

    bool IsEmpty() const {
        return CTxOut::IsEmpty() && nBetValue == 0 && nDate == 0 && nNumber1 == 0 && nNumber2 == 0 && nNumber3 == 0;
    }
};

/** Aggregates the amount of PWR to be minted to pay out all bets as well as dev reward. **/
int64_t GetBlockPayouts(std::vector<CBetOut>& vExpectedPayouts);

/** Validating the payout block using the payout vector. **/
bool IsBlockPayoutsValid(std::vector<CBetOut> vExpectedPayouts, CBlock block, int height);

class CLottoBet
{
public:
    uint32_t nDate;
    uint32_t nNumber1;
    uint32_t nNumber2;
    uint32_t nNumber3;

    // Default constructor.
    CLottoBet() {}

    // Parametrized constructor.
    CLottoBet(int date, int number1, int number2, int number3)
    {
        nDate = date;
		nNumber1 = number1;
		nNumber2 = number2;
		nNumber3 = number3;
    }

    static bool ToOpCode(CLottoBet pb, std::string &opCode);
    static bool FromOpCode(std::string opCode, CLottoBet &pb);
};

/** Get the lotto winning bets from the block chain and return the payout vector. **/
std::vector<CBetOut> GetBetPayouts(int height);

std::time_t lastdrawtime(std::time_t blockTime);

std::string lastdrawdate(std::time_t blockTime);

std::string nextdrawdate(std::time_t blockTime);

std::time_t nextdrawtime(std::time_t blockTime);

#endif // POWERALT_BET_H