#include "stats.h"

bool Stats_Get(RamStats& out) {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms))
        return false;

    out.totalBytes  = ms.ullTotalPhys;
    out.availBytes  = ms.ullAvailPhys;
    out.usedBytes   = ms.ullTotalPhys - ms.ullAvailPhys;
    out.usedPercent = static_cast<int>(ms.dwMemoryLoad);
    out.usedGB      = static_cast<double>(out.usedBytes)  / (1024.0 * 1024.0 * 1024.0);
    out.totalGB     = static_cast<double>(out.totalBytes) / (1024.0 * 1024.0 * 1024.0);
    return true;
}
