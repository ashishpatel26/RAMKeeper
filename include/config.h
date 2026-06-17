#pragma once
#include "common.h"

struct AppConfig {
    // v1.0 fields
    int  thresholdPercent;   // clean when RAM usage > this (default 80)
    int  intervalMinutes;    // force clean every N min (default 30, 0=disabled)
    int  onIdleMinutes;      // clean if idle N+ min (default 5, 0=disabled)
    int  onBootDelaySec;     // clean once N sec after boot (default 60, 0=disabled)
    bool silentMode;         // suppress balloon notifications
    bool autoClean;          // auto-clean enabled
    bool startWithWindows;   // add to registry Run key

    // v1.1 fields
    wchar_t excludeList[256]; // comma-separated exe names to skip from WS trim
    int     cleanHour;        // scheduled clean hour 0-23, -1=disabled
    int     cleanMinute;      // scheduled clean minute 0-59
    int     notifyMinMB;      // min freed MB to show balloon (default 10)
    bool    showStatusOnClean; // open status window after each clean
};

bool Config_Load(AppConfig& cfg);
bool Config_Save(const AppConfig& cfg);
AppConfig Config_Defaults();
bool Config_SetAutostart(bool enable);
