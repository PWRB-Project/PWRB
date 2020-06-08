// Copyright (c) 2018 The Wagerr developers
// Copyright (c) 2020 The powerbalt developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/pwrb-config.h"
#endif

#ifdef WIN32
#define timegm _mkgmtime
#endif

#include "bet.h"
#include "core_io.h"
#include <ctime>

#include "wallet/wallet.h"
#include "spork.h"

#define BTX_FORMAT_VERSION 0x01
#define BTX_HEX_PREFIX "42"

// String lengths for all currently supported op codes.
#define LB_OP_STRLEN  26

/**
 * Takes a payout vector and aggregates the total PWR that is required to pay out all bets.
 * We also calculate and the dev fund rewards.
 *
 * @param vExpectedPayouts  A vector containing all the winning bets that need to be paid out.
 * @return
 */
int64_t GetBlockPayouts(std::vector<CBetOut>& vExpectedPayouts)
{
    CAmount nPayout = 0;
    CAmount totalAmountBet = 0;

    // Set the Dev reward addresses
    std::string devPayoutAddr  = Params().GetConsensus().strDevPayoutAddr;

    // Loop over the payout vector and aggregate values.
    for (unsigned i = 0; i < vExpectedPayouts.size(); i++) {
        CAmount betValue = vExpectedPayouts[i].nBetValue;
        CAmount payValue = vExpectedPayouts[i].nValue;

        totalAmountBet += betValue;
        nPayout += payValue;
    }

    if (vExpectedPayouts.size() > 0) {
        // Calculate the Dev reward.
        CAmount nDevReward  = (CAmount)(nPayout * 0.05);

        // Add dev reward payout to the payout vector.
        vExpectedPayouts.emplace_back(nDevReward, GetScriptForDestination(CBitcoinAddress(devPayoutAddr).Get()));

        nPayout += nDevReward;
    }

    return  nPayout;
}

/**
 * Validates the payout block to ensure all bet payout amounts and payout addresses match their expected values.
 *
 * @param vExpectedPayouts -  The bet payout vector.
 * @param nHeight - The current chain height.
 * @return
 */
bool IsBlockPayoutsValid(std::vector<CBetOut> vExpectedPayouts, CBlock block, int height)
{
    unsigned long size = vExpectedPayouts.size();

    // If we have payouts to validate.
    if (size > 0) {

        CTransaction &tx = block.vtx[1];

        // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
        const CTxIn &txin         = tx.vin[0];
        COutPoint prevout         = txin.prevout;
        unsigned int numStakingTx = 0;
        CAmount stakeAmount       = 0;

        uint256 hashBlock;
        CTransaction txPrev;

        if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
            const CTxOut &prevTxOut = txPrev.vout[prevout.n];
            stakeAmount = prevTxOut.nValue;
        }

        // Count the coinbase and staking vouts in the current block TX.
        CAmount totalStakeAcc = 0;
        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            const CTxOut &txout = tx.vout[i];
            CAmount voutValue   = txout.nValue;

            if (totalStakeAcc < stakeAmount + GetBlockValue(height) - GetMasternodePayment()) {
                numStakingTx++;
            }
            else {
                i = tx.vout.size();
                break;
            }

            totalStakeAcc += voutValue;
        }

        if (vExpectedPayouts.size() + numStakingTx != tx.vout.size()) {
            LogPrintf("%s - Incorrect number of transactions in block %s. Expected: %s. Actual: %s \n", __func__, block.GetHash().ToString(), vExpectedPayouts.size() + numStakingTx, tx.vout.size());
            LogPrintf("%s - Expected Bet Payouts: %s. Expected Stake vOuts: %s. \n", __func__, vExpectedPayouts.size(), numStakingTx);
            return false;
        }

        CAmount totalExpPayout = 0;
        CAmount totalPayout = 0;

        for (unsigned int i = 0; i < vExpectedPayouts.size(); i++) {
            totalExpPayout = totalExpPayout + vExpectedPayouts[i].nValue;
        }
        for (unsigned int i = numStakingTx; i < tx.vout.size(); i++) {
            const CTxOut &txout = tx.vout[i];
            CAmount voutValue   = txout.nValue;            
            totalPayout = totalPayout + voutValue;
        }
        
        if (totalExpPayout != totalPayout) {
            LogPrintf("Validation of bet payout total failed! \n");
            return false;
        }        
        
        // Validate the payout block against the expected payouts vector. If all payout amounts and payout addresses match then we have a valid payout block.
        for (unsigned int k = 0; k < vExpectedPayouts.size(); k++) {

            // Get the expected payout amount and address.
            CAmount vExpectedAmt   = vExpectedPayouts[k].nValue;
            CTxDestination expectedAddr;
            ExtractDestination(vExpectedPayouts[k].scriptPubKey, expectedAddr);
            std::string expectedAddrS = CBitcoinAddress(expectedAddr).ToString();
            
            //for (unsigned int l = numStakingTx; l < tx.vout.size(); l++) {
                
                const CTxOut &txout = tx.vout[numStakingTx + k];
                CAmount voutValue   = txout.nValue;

                LogPrintf("Bet Payout Amount %li  - Expected Bet Payout Amount: %li \n", voutValue, vExpectedAmt);

                // Get the bet payout address.
                CTxDestination betAddr;
                ExtractDestination(tx.vout[numStakingTx + k].scriptPubKey, betAddr);
                std::string betAddrS = CBitcoinAddress(betAddr).ToString();

                LogPrintf("Bet Address %s  - Expected Bet Address: %s \n", betAddrS.c_str(), expectedAddrS.c_str());

                if (vExpectedAmt != voutValue && betAddrS != expectedAddrS) {
                    LogPrintf("Validation of bet payout failed! \n");
                    return false;
                } else {
                    LogPrintf("Validation of bet payout success! \n");
                }
            //}
        }
    }

    return true;
}

/**
 * `ReadBTXFormatVersion` returns -1 if the `opCode` doesn't begin with a valid "BTX" prefix.
 *
 * @param opCode The OpCode as a string
 * @return       The protocal version number
 */
int ReadBTXFormatVersion(std::string opCode)
{
    // Check the first three bytes match the "BTX" format specification.
    if (opCode[0] != 'B') {
        return -1;
    }

    // Check the BTX protocol version number is in range.
    int v = opCode[1];

    // Versions outside the range [1, 254] are not supported.
    return v < 1 || v > 254 ? -1 : v;
}

/**
 * Convert the hex chars for 2 bytes of opCode into uint32_t integer value.
 *
 * @param a First hex char
 * @param b Second hex char
 * @return  32 bit unsigned integer
 */
uint32_t FromChars(unsigned char a, unsigned char b)
{
    uint32_t n = b;
    n <<= 8;
    n += a;

    return n;
}

/**
 * Convert the hex chars for 4 bytes of opCode into uint32_t integer value.
 *
 * @param a First hex char
 * @param b Second hex char
 * @param c Third hex char
 * @param d Fourth hex char
 * @return  32 bit unsigned integer
 */
uint32_t FromChars(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    uint32_t n = d;
    n <<= 8;
    n += c;
    n <<= 8;
    n += b;
    n <<= 8;
    n += a;

    return n;
}

inline uint16_t swap_endianness_16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}
inline uint32_t swap_endianness_32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >>  8) |
            ((x & 0x0000ff00U) <<  8) | ((x & 0x000000ffU) << 24));
}
/**
 * Convert a unsigned 32 bit integer into its hex equivalent with the
 * amount of zero padding given as argument length.
 *
 * @param value  The integer value
 * @param length The size in nr of hex characters
 * @return       Hex string
 */
std::string ToHex(uint32_t value, int length)
{
    std::stringstream strBuffer;
    if (length == 2){
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << value;
    } else if (length == 4){
        uint16_t be_value = (uint16_t)value;
        uint16_t le_value = swap_endianness_16(be_value);
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << le_value;
    } else if (length == 8){
        uint32_t le_value = swap_endianness_32(value);
        strBuffer << std::hex << std::setw(length) << std::setfill('0') << le_value;
    }
    return strBuffer.str();
}

/**
 * Split a CLottoBet OpCode string into byte components and store in peerless
 * bet object.
 *
 * @param opCode The CLottoBet OpCode string
 * @param pe     The CLottoBet object
 * @return       Bool
 */
bool CLottoBet::FromOpCode(std::string opCode, CLottoBet &lb)
{
    // Ensure peerless bet OpCode string is the correct length.
    if (opCode.length() != LB_OP_STRLEN / 2) {
        LogPrintf("%d - LottoBet OpCode length invalid : %s\n", __func__, opCode.length());
        return false;
    }

    // Ensure the peerless bet transaction type is correct.
    if (opCode[2] != plBetTxType) {
        LogPrintf("%d - LottoBet OpCode invalid : %s\n", __func__, opCode[2]);
        return false;
    }

    // Ensure the lotto bet OpCode has the correct BTX format version number.
    if (ReadBTXFormatVersion(opCode) != BTX_FORMAT_VERSION) {
        LogPrintf("%d - LottoBet BTX format invalid : %s\n", __func__, ReadBTXFormatVersion(opCode));
        return false;
    }

    lb.nDate = FromChars(opCode[3], opCode[4], opCode[5], opCode[6]);
    lb.nNumber1 = FromChars(opCode[7], opCode[8]);
    lb.nNumber2 = FromChars(opCode[9], opCode[10]);
    lb.nNumber3 = FromChars(opCode[11], opCode[12]);


    return true;
}

/**
 * Convert CLottoBet object data into hex OPCode string.
 *
 * @param lb     The CLottoBet object
 * @param opCode The CLottoBet OpCode string
 * @return       Bool
 */
bool CLottoBet::ToOpCode(CLottoBet lb, std::string &opCode)
{
    std::string sDate = ToHex(lb.nDate, 8);
    std::string sNumber1 = ToHex(lb.nNumber1, 4);
    std::string sNumber2 = ToHex(lb.nNumber2, 4);
    std::string sNumber3 = ToHex(lb.nNumber3, 4);

    opCode = BTX_HEX_PREFIX "0101" + sDate + sNumber1 + sNumber2 + sNumber3;

    // Ensure lotto bet OpCode string is the correct length.
    if (opCode.length() != LB_OP_STRLEN) {
        LogPrintf("%d - LottoBet OpCode length invalid : %s\n", __func__, opCode.length());
        return false;
    }

    return true;
}

/**
 * Creates the bet payout vector for all winning CLottoBet bets.
 *
 * @return payout vector.
 */
std::vector<CBetOut> GetBetPayouts(int height)
{
    std::vector<CBetOut> vExpectedPayouts;
    std::vector<CBetOut> vCompletedPayouts;
    std::vector<CBetOut> vPendingPayouts;
    
    int nTraverseHeight;

    if (!IsInitialBlockDownload() && (GetTime() - chainActive.Tip()->GetBlockTime() < (60*60*2)) && height % 1500 == 1494) {
        DownloadResultsFile();
        return {};
    } else if (!IsInitialBlockDownload() && (GetTime() - chainActive.Tip()->GetBlockTime() < (60*60*2)) && height % 1500 == 1496) {
        DownloadResultsFile2();
        return {};
    } else if (height % 1500 == 0 && (chainActive.Tip()->GetBlockTime() > lastdrawtime(chainActive.Tip()->GetBlockTime()) + (60*60*16))) {

        std::time_t lastdraw = lastdrawtime(chainActive.Tip()->GetBlockTime());
        std::tm * ptm = gmtime(&lastdraw);
        char chartime[32];
        // Format: Mo, 15.06.2009 20:20:00
        std::strftime(chartime, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);

        LogPrintf("%s : Latest Draw Date=%d - Latest Draw Time=%t  - Next Draw Date=%r \n", __func__, lastdrawdate(chainActive.Tip()->GetBlockTime()), chartime, nextdrawdate(chainActive.Tip()->GetBlockTime()));

        // Look back the chain 6 days for any events and bets.
        CBlockIndex *BlocksIndex = NULL;
        if (height - Params().GetConsensus().nBetBlocksIndexTimespan > Params().GetConsensus().height_last_PoW + 1) {
            nTraverseHeight = height - Params().GetConsensus().nBetBlocksIndexTimespan;
        } else if (height <= 1500) { // don't allow betting payouts until block 1500
            return {};
        } else {
            nTraverseHeight = Params().GetConsensus().height_last_PoW + 1;
        }
        BlocksIndex = chainActive[nTraverseHeight];
        LogPrintf("%s : Setting Block Height of %h \n", __func__, nTraverseHeight);
        
        std::time_t latestDrawStartTime = lastdrawtime(chainActive.Tip()->GetBlockTime());
        std::string latestDrawDate = lastdrawdate(chainActive.Tip()->GetBlockTime());
        
        int year = std::stoi(latestDrawDate.substr(0,4));
        int month = std::stoi(latestDrawDate.substr(5,2));
        int day = std::stoi(latestDrawDate.substr(8,2));
        
        if (day > 10) {
            day -= 1;
            latestDrawDate.replace(8, 2, std::to_string(day));
        }
        else if (day > 1 && day <= 10) {
            day -= 1;
            latestDrawDate.replace(9, 1, std::to_string(day));
        }
        else {
            if (month == 1) {
                month = 12;
                day = 31;
                year -= 1;
                latestDrawDate.replace(5, 2, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
                latestDrawDate.replace(0, 4, std::to_string(year));
            }
            else if (month == 2) {
                month -= 1;
                day = 31;
                latestDrawDate.replace(6, 1, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
            else if (month == 3) {
                month -= 1;
                if (year % 4 == 0) {
                    day = 29;
                } else {
                    day = 28;
                }
                latestDrawDate.replace(6, 1, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
            else if (month == 5 || month == 7 || month == 10) {
                month -= 1;
                day = 30;
                latestDrawDate.replace(6, 1, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
            else if (month == 11) {
                month -= 1;
                day = 31;
                latestDrawDate.replace(5, 2, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
            else if (month == 12) {
                month -= 1;
                day = 30;
                latestDrawDate.replace(5, 2, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
            else {
                month -= 1;
                day = 31;
                latestDrawDate.replace(6, 1, std::to_string(month));
                latestDrawDate.replace(8, 2, std::to_string(day));
            }
        }

        // Read results file for past draws
        std::string NYResult;
        std::string PBResult;
        UniValue officialResults;
        std::string sOfficialResults;
        std::string sOfficialDate;
        std::string resultDate;
        std::string resultNum1;
        std::string resultNum2;
        std::string resultNum3;
        std::string resultNum4;
        std::string resultNum5;

        try {
            NYResult = ReadResultsFile();

            if (!officialResults.read(NYResult) || !officialResults.isArray())
            {
                officialResults = UniValue(UniValue::VARR);
            } else {
                officialResults.get_array();
            }
        
            if (officialResults.empty()) {
                LogPrintf("Official Results Empty \n");
            }

            for (unsigned int idx = 0; idx < officialResults.size(); idx++) {
                const UniValue &val = officialResults[idx];
                const UniValue &o = val.get_obj();

                const UniValue &vDrawdate = find_value(o, "draw_date");
                if (!vDrawdate.isStr())
                    continue;

                const UniValue &vN = find_value(o, "winning_numbers");
                if (!vN.isStr())
                    continue;
            
                sOfficialDate = vDrawdate.get_str();

                LogPrintf("Checking if latestDrawDate of %s matches result date of: %s \n", latestDrawDate, sOfficialDate);
                if (sOfficialDate.find(latestDrawDate) != std::string::npos) {
                    sOfficialResults = vN.get_str();
                    LogPrintf("Official results for %s are: %s \n", sOfficialDate, sOfficialResults);
                    break;
                } else if (sOfficialDate.length() < 10 || std::stoi(sOfficialDate.substr(0,4)) < 2020) {
                    LogPrintf("Official results for %s are unavailable. \n", latestDrawDate);
                    break;
                }
            }
        } catch (const std::exception& e) {
            LogPrintf("Error reading NY results file: %s \n", e.what());
        }

        if (sOfficialResults.length() >= 17) {
            // NY draw results from gov website
            resultDate = sOfficialDate.substr (0,4);
            resultDate.append(sOfficialDate.substr (5,2));
            resultDate.append(sOfficialDate.substr (8,2));
            resultNum1 = sOfficialResults.substr (0,2);
            resultNum2 = sOfficialResults.substr (3,2);
            resultNum3 = sOfficialResults.substr (6,2);
            resultNum4 = sOfficialResults.substr (9,2);
            resultNum5 = sOfficialResults.substr (12,2);
            if (resultNum1.substr(0,1) == "0") {
                resultNum1 = resultNum1.substr(1,1);
            }
            if (resultNum2.substr(0,1) == "0") {
                resultNum2 = resultNum2.substr(1,1);
            }
            if (resultNum3.substr(0,1) == "0") {
                resultNum3 = resultNum3.substr(1,1);
            }
            if (resultNum4.substr(0,1) == "0") {
                resultNum4 = resultNum4.substr(1,1);
            }
            if (resultNum5.substr(0,1) == "0") {
                resultNum5 = resultNum5.substr(1,1);
            }

            LogPrintf("Official NY Gov Results: %s %s %s %s %s %s \n", resultDate, resultNum1, resultNum2, resultNum3, resultNum4, resultNum5);
        } else {	

            // Read PowerBall.com results file for past draws
            try {
                PBResult = ReadResultsFile2();

                if (!officialResults.read(PBResult) || !officialResults.isArray())
                {
                    officialResults = UniValue(UniValue::VARR);
                } else {
                    officialResults.get_array();
                }

                if (officialResults.empty()) {
                    LogPrintf("Official PowerBall.com Results Empty \n");
                }

                for (unsigned int idx = 0; idx < officialResults.size(); idx++) {
                    const UniValue &val = officialResults[idx];
                    const UniValue &o = val.get_obj();

                    const UniValue &vDrawdate = find_value(o, "field_draw_date");
                    if (!vDrawdate.isStr())
                        continue;

                    const UniValue &vN = find_value(o, "field_winning_numbers");
                    if (!vN.isStr())
                        continue;

                    sOfficialDate = vDrawdate.get_str();

                    LogPrintf("Checking if latestDrawDate of %s matches result date of: %s \n", latestDrawDate, sOfficialDate);
                    if (sOfficialDate.find(latestDrawDate) != std::string::npos) {
                        sOfficialResults = vN.get_str();
                        LogPrintf("Official PowerBall.com results for %s are: %s \n", sOfficialDate, sOfficialResults);
                        break;
                    } else if (sOfficialDate.length() < 10 || std::stoi(sOfficialDate.substr(0,4)) < 2020) {
                        LogPrintf("Official PowerBall.com results for %s are unavailable. \n", latestDrawDate);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("Error reading PowerBall.com results file: %s \n", e.what());
            }

            if (sOfficialResults.length() >= 17) {
                // PowerBall.com draw results from PowerBall website
                resultDate = sOfficialDate.substr (0,4);
                resultDate.append(sOfficialDate.substr (5,2));
                resultDate.append(sOfficialDate.substr (8,2));
                resultNum1 = sOfficialResults.substr (0,2);
                resultNum2 = sOfficialResults.substr (3,2);
                resultNum3 = sOfficialResults.substr (6,2);
                resultNum4 = sOfficialResults.substr (9,2);
                resultNum5 = sOfficialResults.substr (12,2);
                if (resultNum1.substr(0,1) == "0") {
                    resultNum1 = resultNum1.substr(1,1);
                }
                if (resultNum2.substr(0,1) == "0") {
                    resultNum2 = resultNum2.substr(1,1);
                }
                if (resultNum3.substr(0,1) == "0") {
                    resultNum3 = resultNum3.substr(1,1);
                }
                if (resultNum4.substr(0,1) == "0") {
                    resultNum4 = resultNum4.substr(1,1);
                }
                if (resultNum5.substr(0,1) == "0") {
                    resultNum5 = resultNum5.substr(1,1);
                }

                LogPrintf("Official PowerBall.com Results: %s %s %s %s %s %s \n", resultDate, resultNum1, resultNum2, resultNum3, resultNum4, resultNum5);
            }
        }

        // If official website results are unavailable for download by peer, use sporked results
        if (sOfficialResults.length() < 17) {
            std::string result = std::to_string(sporkManager.GetSporkValue(SPORK_19_DRAW_RESULT));
            resultDate = result.substr (0,8);
            resultNum1 = result.substr (8,2);
            resultNum2 = result.substr (10,2);
            resultNum3 = result.substr (12,2);
            resultNum4 = result.substr (14,2);
            resultNum5 = result.substr (16,2);
        
            if (resultNum1.substr(0,1) == "0") {
                resultNum1 = resultNum1.substr(1,1);
            }
            if (resultNum2.substr(0,1) == "0") {
                resultNum2 = resultNum2.substr(1,1);
            }
            if (resultNum3.substr(0,1) == "0") {
                resultNum3 = resultNum3.substr(1,1);
            }
            if (resultNum4.substr(0,1) == "0") {
                resultNum4 = resultNum4.substr(1,1);
            }
            if (resultNum5.substr(0,1) == "0") {
                resultNum5 = resultNum5.substr(1,1);
            }
            LogPrintf("Sporked Results: %s %s %s %s %s %s \n", resultDate, resultNum1, resultNum2, resultNum3, resultNum4, resultNum5);
        }
        
        double nOneNumberOdds = 12;
        double nTwoNumberOdds = 200;
        double nThreeNumberOdds = 3500;

        // Traverse the block chain to find bets.
        LogPrintf("%s : Traverse the block chain to find bets.\n", __func__);
        while (BlocksIndex) {
            CBlock block;
            ReadBlockFromDisk(block, BlocksIndex);
            std::time_t transactionTime = block.nTime;
            
            for (CTransaction &tx : block.vtx) {
                // Check all TX vouts for an OP RETURN.
                for (unsigned int i = 0; i < tx.vout.size(); i++) {

                    const CTxOut &txout = tx.vout[i];
                    std::string scriptPubKey = ScriptToAsmStr(txout.scriptPubKey);
                    CAmount betAmount = txout.nValue;

                    if (scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {

                        // Get the OP CODE from the transaction scriptPubKey.
                        std::vector<unsigned char> vOpCode = ParseHex(scriptPubKey.substr(9, std::string::npos));
                        std::string opCode(vOpCode.begin(), vOpCode.end());

                        // Only payout bets that are in the specified range (MaxBetPayoutRange).
                        if (betAmount >= (Params().GetConsensus().nMinBetPayoutRange * COIN) && betAmount <= (Params().GetConsensus().nMaxBetPayoutRange * COIN)) {

                            // Bet OP RETURN transaction.
                            CLottoBet lb;
                            if (CLottoBet::FromOpCode(opCode, lb)) {

                                LogPrintf("%s : Block Timestamp: %s : Found a bet for date : %s - Using Selections : [%s] [%s] [%s] \n", __func__, transactionTime, lb.nDate, lb.nNumber1, lb.nNumber2, lb.nNumber3);
                                CAmount payout = 0 * COIN;

                                // Is the bet a winning bet?
                                std::ostringstream sDate;
                                sDate << lb.nDate;

                                bool isWinner = true;
                                
                                if (lb.nNumber1 != 0 && lb.nNumber1 != (unsigned int) std::stoi(resultNum1) && lb.nNumber1 != (unsigned int) std::stoi(resultNum2) && lb.nNumber1 != (unsigned int) std::stoi(resultNum3) && lb.nNumber1 != (unsigned int) std::stoi(resultNum4) && lb.nNumber1 != (unsigned int) std::stoi(resultNum5)) {
                                    isWinner = false;
                                }
                                if (lb.nNumber2 != 0 && lb.nNumber2 != (unsigned int) std::stoi(resultNum1) && lb.nNumber2 != (unsigned int) std::stoi(resultNum2) && lb.nNumber2 != (unsigned int) std::stoi(resultNum3) && lb.nNumber2 != (unsigned int) std::stoi(resultNum4) && lb.nNumber2 != (unsigned int) std::stoi(resultNum5)) {
                                    isWinner = false;
                                }
                                if (lb.nNumber3 != 0 && lb.nNumber3 != (unsigned int) std::stoi(resultNum1) && lb.nNumber3 != (unsigned int) std::stoi(resultNum2) && lb.nNumber3 != (unsigned int) std::stoi(resultNum3) && lb.nNumber3 != (unsigned int) std::stoi(resultNum4) && lb.nNumber3 != (unsigned int) std::stoi(resultNum5)) {
                                    isWinner = false;
                                }
                                if ((lb.nNumber1 == lb.nNumber2 && lb.nNumber1 != 0)||(lb.nNumber1 == lb.nNumber3 && lb.nNumber1 != 0)||(lb.nNumber2 == lb.nNumber3 && lb.nNumber2 != 0)) {
                                    isWinner = false;
                                }
                                if (lb.nNumber1 > 69 || lb.nNumber2 > 69 || lb.nNumber3 > 69) {
                                    isWinner = false;
                                }
                                if (lb.nNumber1 == 0 && lb.nNumber2 == 0 && lb.nNumber3 == 0) {
                                    isWinner = false;
                                }

                                // If bet was placed less than X mins before event start or after event start discard it.
                                if ((unsigned int) transactionTime > (latestDrawStartTime - Params().GetConsensus().nBetPlaceTimeoutBlocks)) {
                                    LogPrintf("%s : Bet placed too late. Draw Started: %t - Bet Placed - %u \n", __func__, latestDrawStartTime, transactionTime);
                                    isWinner = false;
                                }

                                // If bet isn't for latest draw, discard it
                                if (resultDate != sDate.str()) {
                                    LogPrintf("%s : Bet not for latest draw: Bet for draw date: %t - Latest Results Date: %u \n", __func__, sDate.str(), resultDate);
                                    isWinner = false;
                                }                                
                                
                                if (resultDate == sDate.str() && isWinner) {

                                    // Calculate winnings.
                                    if (lb.nNumber3 > 0 && lb.nNumber2 > 0 && lb.nNumber1 > 0) {
                                        payout = betAmount * nThreeNumberOdds;
                                    }
                                    else if ((lb.nNumber1 > 0 && lb.nNumber2 > 0)||(lb.nNumber1 > 0 && lb.nNumber3 > 0)||(lb.nNumber2 > 0 && lb.nNumber3 > 0)) {
                                        payout = betAmount * nTwoNumberOdds;
                                    }
                                    else if (lb.nNumber1 > 0 || lb.nNumber2 > 0 || lb.nNumber3 > 0) {
                                       payout = betAmount * nOneNumberOdds;
                                    }
                                    else {
                                        payout = 0;
                                    }

                                    // Get the users payout address from the vin of the bet TX they used to place the bet.
                                    CTxDestination payoutAddress;
                                    const CTxIn &txin = tx.vin[0];
                                    COutPoint prevout = txin.prevout;

                                    uint256 hashBlock;
                                    CTransaction txPrev;
                                    if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {
                                        ExtractDestination( txPrev.vout[prevout.n].scriptPubKey, payoutAddress );
                                    }

                                    // Only add valid payouts to the vExpectedPayouts vector.
                                    if (payout > 0 && payout <= (Params().GetConsensus().nMaxWinnerPayout * COIN)) {
                                        // Add winning bet payout to the bet vector.
                                        vExpectedPayouts.emplace_back(payout, GetScriptForDestination(CBitcoinAddress(payoutAddress).Get()), betAmount);

                                        LogPrintf("Winning Line - PAYOUT\n");
                                        LogPrintf("AMOUNT: %li \n", payout);
                                        LogPrintf("NUMBER1: %li \n", lb.nNumber1);
                                        LogPrintf("NUMBER2: %li \n", lb.nNumber2);
                                        LogPrintf("NUMBER3: %li \n", lb.nNumber3);
                                        LogPrintf("ADDRESS: %s \n", CBitcoinAddress( payoutAddress ).ToString().c_str());
                                    } else {
                                        LogPrintf("%s : Bet payout of %t outside permitted range. \n", __func__, payout);
                                        isWinner = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            CTransaction &cbtx = block.vtx[1]; 
            if (cbtx.vout.size() > 1 && transactionTime >= latestDrawStartTime) {        

                // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
                const CTxIn &txin = cbtx.vin[0];
                COutPoint cbprevout = txin.prevout;
                unsigned int numStakingTx = 0;
                CAmount stakeAmount = 0;

                uint256 cbHashBlock;
                CTransaction cbTxPrev;

                if (GetTransaction(cbprevout.hash, cbTxPrev, cbHashBlock, true)) {
                    const CTxOut &prevCbTxOut = cbTxPrev.vout[cbprevout.n];
                    stakeAmount = prevCbTxOut.nValue;
                }

                // Count the coinbase and staking vouts in the current block TX.
                CAmount totalStakeAcc = 0;
                for (unsigned int i = 0; i < cbtx.vout.size(); i++) {
                    const CTxOut &cbtxout = cbtx.vout[i];
                    CAmount voutValue = cbtxout.nValue;
                                        
                    if (totalStakeAcc < stakeAmount + GetBlockValue(nTraverseHeight) - GetMasternodePayment()) {
                        numStakingTx++;
                    }
                    else {
                        i = cbtx.vout.size();
                        break;
                    }

                    totalStakeAcc += voutValue;
                }
                
                // Add paid out bets to vCompletedPayouts vector
                for (unsigned int j = 0; j < cbtx.vout.size() - numStakingTx; j++) {
                    unsigned int i = numStakingTx + j;

                    const CTxOut &cbtxout = cbtx.vout[i];
                    CAmount voutValue = cbtxout.nValue;

                    // Get the bet payout address.
                    CTxDestination betAddr;
                    ExtractDestination(cbtx.vout[i].scriptPubKey, betAddr);
                    std::string betAddrS = CBitcoinAddress(betAddr).ToString();

                    if ( i == (cbtx.vout.size() - 1) and voutValue == (GetMasternodePayment())) {
                        LogPrintf("GetBetPayouts: Masternode Address: %s  - Masternode Payment Amount: %s \n", betAddrS.c_str(),voutValue);
                    } else {
                        LogPrintf("GetBetPayouts: Bet Address: %s  - Processed Bet Amount: %s \n", betAddrS.c_str(),voutValue);
                        vCompletedPayouts.emplace_back(voutValue, cbtx.vout[i].scriptPubKey, 0);
                    }
                }
            }
                            
            nTraverseHeight ++;
            BlocksIndex = chainActive.Next(BlocksIndex);
        }
        
        unsigned long vExpectedSize = vExpectedPayouts.size();
        unsigned long vCompletedSize = vCompletedPayouts.size();

        // If we have bets to be paid
        if (vExpectedSize > vCompletedSize) {    
            
            bool betPaid = false;
            
            LogPrintf("Pending Payouts Initial Size: %s \n", vPendingPayouts.size());
            // Iterate through every bet payout expected
            for (unsigned int j = 0; j < vExpectedSize; j++) {

                // Get the expected payout address.
                CTxDestination expectedAddr;
                ExtractDestination(vExpectedPayouts[j].scriptPubKey, expectedAddr);
                std::string expectedAddrS = CBitcoinAddress(expectedAddr).ToString();

                // Get the expected payout amount.
                CAmount expectedValue;
                expectedValue = vExpectedPayouts[j].nValue;

                // Get the bet amount.
                CAmount expectedBet;
                expectedBet = vExpectedPayouts[j].nBetValue;

                LogPrintf("Expected Bet Address: %s - Expected Bet Amount: %s - Expected Winning Amount: %s \n", expectedAddrS.c_str(), expectedBet, expectedValue);

                // For every bet already paid out, 
                for (unsigned int k = 0; k < vCompletedSize; k++) {

                    // Get the completed payout address.
                    CTxDestination completedAddr;
                    ExtractDestination(vCompletedPayouts[k].scriptPubKey, completedAddr);
                    std::string completedAddrS = CBitcoinAddress(completedAddr).ToString();

                    // Get the completed payout amount.
                    CAmount completedValue;
                    completedValue = vCompletedPayouts[k].nValue;
            
                    if (completedValue == expectedValue && completedAddrS == expectedAddrS) {
                        betPaid = true;
                        k = vCompletedSize;
                        LogPrintf("Completed Bet Address: %s - Completed Bet Amount: %s \n", completedAddrS.c_str(), completedValue);
                    }
                }

                if (!betPaid) {
                    LogPrintf("Pending Payouts Adding Item %s. Value: %t \n", j + 1, vExpectedPayouts[j].nValue);
                    vPendingPayouts.emplace_back(vExpectedPayouts[j].nValue, vExpectedPayouts[j].scriptPubKey, vExpectedPayouts[j].nBetValue);
                    vExpectedPayouts[j].nBetValue = 0;
                    LogPrintf("Pending Payouts New Size: %s \n", vPendingPayouts.size());
                }
                
                betPaid = false;
            }
        }
    }
    
    return vPendingPayouts;
}

std::time_t lastdrawtime(std::time_t blockTime)
{
    std::tm * lDrawTime = std::gmtime(&blockTime);
    
    std::tm wsTime = {};
    wsTime.tm_year = lDrawTime->tm_year;
    wsTime.tm_mon  = lDrawTime->tm_mon;
    wsTime.tm_mday = lDrawTime->tm_mday;
    wsTime.tm_wday = lDrawTime->tm_wday;
    wsTime.tm_hour = lDrawTime->tm_hour;
    wsTime.tm_min = lDrawTime->tm_min;
    wsTime.tm_sec = lDrawTime->tm_sec;

    if (wsTime.tm_wday == 0 && (wsTime.tm_hour < 2 || (wsTime.tm_hour == 2 && wsTime.tm_min < 59))) {
        wsTime.tm_mday -= 3;
    }
    else if (wsTime.tm_wday == 4 && (wsTime.tm_hour < 2 || (wsTime.tm_hour == 2 && wsTime.tm_min < 59))) {
        wsTime.tm_mday -= 4;
    }
    else if (wsTime.tm_wday >= 0 && wsTime.tm_wday <= 3) {
        wsTime.tm_mday -= wsTime.tm_wday;
    }
    else if (wsTime.tm_wday >= 4 && wsTime.tm_wday <= 6) {
        wsTime.tm_mday -= (wsTime.tm_wday - 4);
    }
    wsTime.tm_hour = 02;
    wsTime.tm_min = 59;
    wsTime.tm_sec = 0;
    wsTime.tm_isdst = -1;

    timegm(&wsTime);
    
    return timegm(&wsTime);
}

std::string lastdrawdate(std::time_t blockTime)
{
    std::tm * lDrawDate = std::gmtime(&blockTime);
    
    std::tm wsTime = {};
    wsTime.tm_year = lDrawDate->tm_year;
    wsTime.tm_mon  = lDrawDate->tm_mon;
    wsTime.tm_mday = lDrawDate->tm_mday;
    wsTime.tm_wday = lDrawDate->tm_wday;
    wsTime.tm_hour = lDrawDate->tm_hour;
    wsTime.tm_min = lDrawDate->tm_min;
    wsTime.tm_sec = lDrawDate->tm_sec;

    if (wsTime.tm_wday == 0 && (wsTime.tm_hour < 2 || (wsTime.tm_hour == 2 && wsTime.tm_min < 59))) {
        wsTime.tm_mday -= 3;
    }
    else if (wsTime.tm_wday == 4 && (wsTime.tm_hour < 2 || (wsTime.tm_hour == 2 && wsTime.tm_min < 59))) {
        wsTime.tm_mday -= 4;
    }
    else if (wsTime.tm_wday >= 0 && wsTime.tm_wday <= 3) {
        wsTime.tm_mday -= wsTime.tm_wday;
    }
    else if (wsTime.tm_wday >= 4 && wsTime.tm_wday <= 6) {
        wsTime.tm_mday -= (wsTime.tm_wday - 4);
    }
    wsTime.tm_hour = 02;
    wsTime.tm_min = 59;
    wsTime.tm_sec = 0;
    wsTime.tm_isdst = -1;

    timegm(&wsTime);
    
    char datechar[12];

    std::strftime(datechar, sizeof(datechar), "%Y-%m-%d", &wsTime);

    std::string datestr(datechar);
    
    return datestr;
}

std::time_t nextdrawtime(std::time_t blockTime)
{
    std::time_t tLastDrawTime = lastdrawtime(blockTime);
    std::tm * nDrawTime = std::gmtime(&tLastDrawTime);
    
    std::tm wsTime = {};
    wsTime.tm_year = nDrawTime->tm_year;
    wsTime.tm_mon  = nDrawTime->tm_mon;
    wsTime.tm_mday = nDrawTime->tm_mday;
    wsTime.tm_wday = nDrawTime->tm_wday;
    wsTime.tm_hour = nDrawTime->tm_hour;
    wsTime.tm_min = nDrawTime->tm_min;
    wsTime.tm_sec = nDrawTime->tm_sec;
    wsTime.tm_hour = 02;
    wsTime.tm_min = 59;
    wsTime.tm_sec = 0;
    wsTime.tm_isdst = -1;

    if (wsTime.tm_wday == 0) {
        wsTime.tm_mday += 4;
    }
    else if (wsTime.tm_wday == 4) {
        wsTime.tm_mday += 3;
    }

    timegm(&wsTime);

    return timegm(&wsTime);
}
std::string nextdrawdate(std::time_t blockTime)
{
    std::time_t tLastDrawTime = lastdrawtime(blockTime);
    std::tm * nDrawDate = std::gmtime(&tLastDrawTime);
    
    std::tm wsTime = {};
    wsTime.tm_year = nDrawDate->tm_year;
    wsTime.tm_mon  = nDrawDate->tm_mon;
    wsTime.tm_mday = nDrawDate->tm_mday;
    wsTime.tm_wday = nDrawDate->tm_wday;
    wsTime.tm_hour = nDrawDate->tm_hour;
    wsTime.tm_min = nDrawDate->tm_min;
    wsTime.tm_sec = nDrawDate->tm_sec;
    wsTime.tm_hour = 22;
    wsTime.tm_min = 59;
    wsTime.tm_sec = 0;
    wsTime.tm_isdst = -1;

    if (wsTime.tm_wday == 0) {
        wsTime.tm_mday += 3;
    }
    else if (wsTime.tm_wday == 4) {
        wsTime.tm_mday += 2;
    }    
        
    timegm(&wsTime);
    
    char datechar[12];

    std::strftime(datechar, sizeof(datechar), "%Y%m%d", &wsTime);

    std::string datestr(datechar);
    
    return datestr;
}
