#pragma once
#include "common.h"

struct RamStats {
    ULONGLONG totalBytes;
    ULONGLONG availBytes;
    ULONGLONG usedBytes;
    int       usedPercent;  // 0-100
    double    usedGB;
    double    totalGB;
};

bool Stats_Get(RamStats& out);
