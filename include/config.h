#pragma once
#include "common.h"

struct AppConfig {
    int  thresholdPercent;   // clean when RAM usage > this (default 80)
    int  intervalMinutes;    // force clean every N min (default 30, 0=disabled)
    int  onIdleMinutes;      // clean if idle N+ min (default 5, 0=disabled)
    int  onBootDelaySec;     // clean once N sec after boot (default 60, 0=disabled)
    bool silentMode;         // suppress balloon notifications
    bool autoClean;          // auto-clean enabled
    bool startWithWindows;   // add to registry Run key
};

// Load from %APPDATA%\RAMKeeper\config.ini (creates file with defaults if missing)
bool Config_Load(AppConfig& cfg);

// Persist to %APPDATA%\RAMKeeper\config.ini
bool Config_Save(const AppConfig& cfg);

// Default values
AppConfig Config_Defaults();

// Toggle autostart registry key
bool Config_SetAutostart(bool enable);
