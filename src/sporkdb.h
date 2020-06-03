// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2020 The PWRDev developers
// Copyright (c) 2020 The powerbalt developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PWRB_CSPORKDB_H
#define PWRB_CSPORKDB_H

#include <boost/filesystem/path.hpp>
#include "dbwrapper.h"
#include "spork.h"

class CSporkDB : public CDBWrapper
{
public:
    CSporkDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CSporkDB(const CSporkDB&);
    void operator=(const CSporkDB&);

public:
    bool WriteSpork(const SporkId nSporkId, const CSporkMessage& spork);
    bool ReadSpork(const SporkId nSporkId, CSporkMessage& spork);
    bool SporkExists(const SporkId nSporkId);
};


#endif //PWRB_CSPORKDB_H
