#include "common.h"
#include "tray.h"
#include "cleaner.h"
#include "stats.h"
#include "config.h"
#include "settings.h"
#include "history.h"
#include "status.h"
#include <strsafe.h>
#include <shlobj.h>

// ---- Application state -------------------------------------------------------

struct AppState {
    AppConfig cfg;
    ULONGLONG lastCleanTick;
    ULONGLONG bootCleanDoneTick;
    bool      bootCleanScheduled;
    bool      isAdmin;
    bool      adminWarned;       // one-time "limited mode" balloon
    int       lastScheduledDay;  // guard: prevent re-trigger in same day
};

static AppState g_app{};
static HWND     g_hwnd = nullptr;

// ---- Forward declarations ----------------------------------------------------

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static void DoClean(HWND hwnd);
static void ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* msg);

// ---- Helpers -----------------------------------------------------------------

static bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION e{};
    DWORD sz;
    bool r = GetTokenInformation(tok, TokenElevation, &e, sizeof(e), &sz) && e.TokenIsElevated;
    CloseHandle(tok);
    return r;
}

static void UpdateTray() {
    RamStats s{};
    if (Stats_Get(s)) {
        Tray_Update(g_hwnd, s.usedGB, s.totalGB, s.usedPercent);
        History_Push(s.usedPercent);
        Status_Update();
    }
}

static bool IsUserIdle(ULONGLONG thresholdMs) {
    LASTINPUTINFO lii{};
    lii.cbSize = sizeof(lii);
    GetLastInputInfo(&lii);
    // ponytail: GetLastInputInfo uses 32-bit tick domain; ~49d rollover benign in practice
    return (GetTickCount64() - lii.dwTime) >= thresholdMs;
}

// ---- Clean -------------------------------------------------------------------

static void DoClean(HWND hwnd) {
    RamStats before{}, after{};
    Stats_Get(before);

    int trimmed = Cleaner_Run(g_app.cfg.excludeList[0] ? g_app.cfg.excludeList : nullptr);

    Stats_Get(after);
    g_app.lastCleanTick = GetTickCount64();

    double freed = static_cast<double>(
        static_cast<LONGLONG>(after.availBytes) - static_cast<LONGLONG>(before.availBytes))
        / (1024.0 * 1024.0);
    if (freed < 0.0) freed = 0.0;

    History_LogClean(freed, trimmed);

    // Persist to clean.log
    wchar_t logPath[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, logPath);
    StringCchCatW(logPath, MAX_PATH, L"\\RAMKeeper\\clean.log");
    History_WriteCleanLog(logPath, freed, trimmed);

    if (!g_app.cfg.silentMode) {
        if (!g_app.isAdmin && !g_app.adminWarned) {
            g_app.adminWarned = true;
            ShowBalloon(hwnd, L"RAMKeeper — Limited mode",
                L"Not running as admin — standby & cache cleanup skipped. Right-click tray → Run as Administrator.");
        } else if (freed >= static_cast<double>(g_app.cfg.notifyMinMB)) {
            wchar_t msg[128]{};
            StringCchPrintfW(msg, ARRAYSIZE(msg),
                L"Freed %.0f MB  (%d processes trimmed)", freed, trimmed);
            ShowBalloon(hwnd, L"RAM Cleaned", msg);
        }
    }

    UpdateTray();

    if (g_app.cfg.showStatusOnClean)
        Status_Show(hwnd);
}

// ---- Balloon notification ----------------------------------------------------

static void ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* msg) {
    (void)hwnd;
    NOTIFYICONDATAW nid{};
    nid.cbSize      = sizeof(nid);
    nid.hWnd        = g_hwnd;
    nid.uID         = IDI_TRAY;
    nid.uFlags      = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title);
    StringCchCopyW(nid.szInfo,      ARRAYSIZE(nid.szInfo),      msg);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ---- Timer tick (1 second) ---------------------------------------------------

static void OnTimer() {
    UpdateTray();

    if (!g_app.cfg.autoClean) return;

    ULONGLONG now = GetTickCount64();

    // One-time boot-delay clean
    if (!g_app.bootCleanScheduled && g_app.cfg.onBootDelaySec > 0) {
        g_app.bootCleanScheduled = true;
        g_app.bootCleanDoneTick  = now + static_cast<ULONGLONG>(g_app.cfg.onBootDelaySec) * 1000;
    }
    if (g_app.bootCleanDoneTick && now >= g_app.bootCleanDoneTick) {
        g_app.bootCleanDoneTick = 0;
        DoClean(g_hwnd);
        return;
    }

    // Scheduled clean (once per day at configured hour:minute)
    if (g_app.cfg.cleanHour >= 0) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        if (t.wHour   == (WORD)g_app.cfg.cleanHour &&
            t.wMinute == (WORD)g_app.cfg.cleanMinute) {
            if (t.wDay != (WORD)g_app.lastScheduledDay) {
                g_app.lastScheduledDay = t.wDay;
                DoClean(g_hwnd);
                return;
            }
        }
    }

    // Interval-based clean
    if (g_app.cfg.intervalMinutes > 0) {
        ULONGLONG intervalMs = static_cast<ULONGLONG>(g_app.cfg.intervalMinutes) * 60 * 1000;
        if ((now - g_app.lastCleanTick) >= intervalMs) {
            DoClean(g_hwnd);
            return;
        }
    }

    // Threshold-based clean (5-min minimum re-clean gap)
    if (g_app.cfg.thresholdPercent > 0) {
        RamStats s{};
        if (Stats_Get(s) && s.usedPercent >= g_app.cfg.thresholdPercent) {
            ULONGLONG minGapMs = 5ULL * 60 * 1000;
            if ((now - g_app.lastCleanTick) >= minGapMs) {
                DoClean(g_hwnd);
                return;
            }
        }
    }

    // Idle-based clean
    if (g_app.cfg.onIdleMinutes > 0) {
        ULONGLONG idleMs = static_cast<ULONGLONG>(g_app.cfg.onIdleMinutes) * 60 * 1000;
        if (IsUserIdle(idleMs) && (now - g_app.lastCleanTick) >= idleMs) {
            DoClean(g_hwnd);
        }
    }
}

// ---- Window procedure --------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_POLL, 1000, nullptr);
        RegisterHotKey(hwnd, HOTKEY_CLEAN, MOD_CONTROL | MOD_ALT, 'R');
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_POLL);
        UnregisterHotKey(hwnd, HOTKEY_CLEAN);
        Status_Hide();
        Tray_Destroy(hwnd);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wp == TIMER_POLL) OnTimer();
        return 0;

    case WM_HOTKEY:
        if (wp == HOTKEY_CLEAN) {
            DoClean(hwnd);
            // Brief tooltip flash confirms hotkey fired
            if (g_app.cfg.silentMode)
                ShowBalloon(hwnd, L"RAMKeeper", L"Clean triggered (Ctrl+Alt+R)");
        }
        return 0;

    case WM_TRAYICON:
        switch (LOWORD(lp)) {
        case WM_RBUTTONUP:
            Tray_ShowContextMenu(hwnd, g_app.cfg.autoClean);
            break;
        case WM_LBUTTONUP:
            // Single left-click = toggle status window
            if (Status_IsVisible()) Status_Hide();
            else                    Status_Show(hwnd);
            break;
        case WM_LBUTTONDBLCLK:
            // Double-click = clean now (fires after WM_LBUTTONUP which opens status;
            // status will close on lose focus from DoClean notification, acceptable)
            DoClean(hwnd);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_CLEAN_NOW:
            DoClean(hwnd);
            break;
        case IDM_STATUS:
            if (Status_IsVisible()) Status_Hide();
            else                    Status_Show(hwnd);
            break;
        case IDM_AUTO_TOGGLE:
            g_app.cfg.autoClean = !g_app.cfg.autoClean;
            Config_Save(g_app.cfg);
            break;
        case IDM_SETTINGS:
            Settings_Show(hwnd, g_app.cfg);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static UINT g_wmTaskbarCreated = 0;

static LRESULT CALLBACK WndProcDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_wmTaskbarCreated && msg == g_wmTaskbarCreated) {
        Tray_Init(hwnd);
        return 0;
    }
    return WndProc(hwnd, msg, wp, lp);
}

// ---- WinMain -----------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, APP_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProcDispatch;
    wc.hInstance     = hInst;
    wc.lpszClassName = APP_NAME;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY));
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, APP_NAME, APP_NAME,
                              0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    Config_Load(g_app.cfg);
    Config_SetAutostart(g_app.cfg.startWithWindows);
    g_app.isAdmin = IsElevated();

    if (!Tray_Init(g_hwnd)) {
        DestroyWindow(g_hwnd);
        return 1;
    }

    UpdateTray();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mutex) CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}
