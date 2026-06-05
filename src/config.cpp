#include "config.h"
#include <shlobj.h>
#include <strsafe.h>

static void GetConfigPath(wchar_t* path, DWORD len) {
    wchar_t appdata[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    StringCchPrintfW(path, len, L"%s\\RAMKeeper\\config.ini", appdata);
}

static void EnsureDir() {
    wchar_t appdata[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    wchar_t dir[MAX_PATH]{};
    StringCchPrintfW(dir, MAX_PATH, L"%s\\RAMKeeper", appdata);
    CreateDirectoryW(dir, nullptr); // no-op if exists
}

AppConfig Config_Defaults() {
    AppConfig c{};
    c.thresholdPercent = 80;
    c.intervalMinutes  = 30;
    c.onIdleMinutes    = 5;
    c.onBootDelaySec   = 60;
    c.silentMode       = false;
    c.autoClean        = true;
    c.startWithWindows = false;
    return c;
}

// Helper: read int from INI with fallback
static int ReadInt(const wchar_t* path, const wchar_t* section,
                   const wchar_t* key, int def) {
    return static_cast<int>(
        GetPrivateProfileIntW(section, key, def, path));
}

bool Config_Load(AppConfig& cfg) {
    cfg = Config_Defaults();

    wchar_t path[MAX_PATH]{};
    GetConfigPath(path, MAX_PATH);

    // If file missing, save defaults and return
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        EnsureDir();
        Config_Save(cfg);
        return true;
    }

    cfg.thresholdPercent = ReadInt(path, L"Clean", L"threshold_percent", 80);
    cfg.intervalMinutes  = ReadInt(path, L"Clean", L"interval_minutes",  30);
    cfg.onIdleMinutes    = ReadInt(path, L"Clean", L"on_idle_minutes",    5);
    cfg.onBootDelaySec   = ReadInt(path, L"Clean", L"on_boot_delay",      60);
    cfg.silentMode       = ReadInt(path, L"Clean", L"silent_mode",        0) != 0;
    cfg.autoClean        = ReadInt(path, L"Clean", L"auto_clean",         1) != 0;
    cfg.startWithWindows = ReadInt(path, L"App",   L"start_with_windows", 0) != 0;
    return true;
}

bool Config_Save(const AppConfig& cfg) {
    EnsureDir();
    wchar_t path[MAX_PATH]{};
    GetConfigPath(path, MAX_PATH);

    wchar_t buf[32]{};

    auto WriteInt = [&](const wchar_t* sec, const wchar_t* key, int val) {
        StringCchPrintfW(buf, 32, L"%d", val);
        WritePrivateProfileStringW(sec, key, buf, path);
    };

    WriteInt(L"Clean", L"threshold_percent", cfg.thresholdPercent);
    WriteInt(L"Clean", L"interval_minutes",  cfg.intervalMinutes);
    WriteInt(L"Clean", L"on_idle_minutes",   cfg.onIdleMinutes);
    WriteInt(L"Clean", L"on_boot_delay",     cfg.onBootDelaySec);
    WriteInt(L"Clean", L"silent_mode",       cfg.silentMode ? 1 : 0);
    WriteInt(L"Clean", L"auto_clean",        cfg.autoClean  ? 1 : 0);
    WriteInt(L"App",   L"start_with_windows",cfg.startWithWindows ? 1 : 0);
    return true;
}

bool Config_SetAutostart(bool enable) {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return false;

    if (enable) {
        wchar_t exePath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0) { RegCloseKey(key); return false; }
        RegSetValueExW(key, APP_NAME, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(exePath),
                       static_cast<DWORD>((len + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, APP_NAME);
    }

    RegCloseKey(key);
    return true;
}
