#include "common.h"
#include "tray.h"
#include "cleaner.h"
#include "stats.h"
#include "config.h"
#include <strsafe.h>

// ---- Application state -------------------------------------------------------

struct AppState {
    AppConfig cfg;
    ULONGLONG lastCleanTick;      // GetTickCount64 when we last cleaned
    ULONGLONG bootCleanDoneTick;  // 0 = not done yet
    bool      bootCleanScheduled;
};

static AppState g_app{};
static HWND     g_hwnd = nullptr;

// ---- Forward declarations ----------------------------------------------------

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static void DoClean(HWND hwnd);
static void ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* msg);

// ---- Helpers -----------------------------------------------------------------

static void UpdateTray() {
    RamStats s{};
    if (Stats_Get(s))
        Tray_Update(g_hwnd, s.usedGB, s.totalGB, s.usedPercent);
}

static bool IsUserIdle(ULONGLONG thresholdMs) {
    LASTINPUTINFO lii{};
    lii.cbSize = sizeof(lii);
    GetLastInputInfo(&lii);
    // GetLastInputInfo uses GetTickCount (32-bit) domain; compare in same domain
    return (GetTickCount64() - lii.dwTime) >= thresholdMs;
}

// ---- Clean -------------------------------------------------------------------

static void DoClean(HWND hwnd) {
    RamStats before{}, after{};
    Stats_Get(before);

    int trimmed = Cleaner_Run();

    Stats_Get(after);
    g_app.lastCleanTick = GetTickCount64();

    if (!g_app.cfg.silentMode) {
        double freed = static_cast<double>(after.availBytes - before.availBytes)
                       / (1024.0 * 1024.0);
        if (freed > 10.0) { // only notify if meaningful (>10 MB freed)
            wchar_t msg[128]{};
            StringCchPrintfW(msg, ARRAYSIZE(msg),
                L"Freed %.0f MB  (%d processes trimmed)", freed, trimmed);
            ShowBalloon(hwnd, L"RAM Cleaned", msg);
        }
    }

    UpdateTray();
}

// ---- Balloon notification ----------------------------------------------------

static void ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* msg) {
    (void)hwnd;
    NOTIFYICONDATAW nid{};
    nid.cbSize    = sizeof(nid);
    nid.hWnd      = g_hwnd;
    nid.uID       = IDI_TRAY;
    nid.uFlags    = NIF_INFO;
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

    // Interval-based clean
    if (g_app.cfg.intervalMinutes > 0) {
        ULONGLONG intervalMs = static_cast<ULONGLONG>(g_app.cfg.intervalMinutes) * 60 * 1000;
        if ((now - g_app.lastCleanTick) >= intervalMs) {
            DoClean(g_hwnd);
            return;
        }
    }

    // Threshold-based clean
    if (g_app.cfg.thresholdPercent > 0) {
        RamStats s{};
        if (Stats_Get(s) && s.usedPercent >= g_app.cfg.thresholdPercent) {
            // Only clean once per interval (5 min minimum gap)
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
        SetTimer(hwnd, TIMER_POLL, 1000, nullptr); // 1-second tick
        RegisterHotKey(hwnd, HOTKEY_CLEAN, MOD_CONTROL | MOD_ALT, 'R');
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_POLL);
        UnregisterHotKey(hwnd, HOTKEY_CLEAN);
        Tray_Destroy(hwnd);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wp == TIMER_POLL) OnTimer();
        return 0;

    case WM_HOTKEY:
        if (wp == HOTKEY_CLEAN) DoClean(hwnd);
        return 0;

    case WM_TRAYICON:
        switch (LOWORD(lp)) {
        case WM_RBUTTONUP:
            Tray_ShowContextMenu(hwnd, g_app.cfg.autoClean);
            break;
        case WM_LBUTTONDBLCLK:
            // Double-click tray icon = clean now
            DoClean(hwnd);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_CLEAN_NOW:
            DoClean(hwnd);
            break;
        case IDM_AUTO_TOGGLE:
            g_app.cfg.autoClean = !g_app.cfg.autoClean;
            Config_Save(g_app.cfg);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        // IDM_SETTINGS — placeholder for future settings dialog
        }
        return 0;

    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// WM_TASKBARCREATED registered at runtime — cannot be a switch case label
static UINT g_wmTaskbarCreated = 0;

static LRESULT CALLBACK WndProcDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Re-add tray icon when Explorer restarts (e.g. after crash)
    if (g_wmTaskbarCreated && msg == g_wmTaskbarCreated) {
        Tray_Init(hwnd);
        return 0;
    }
    return WndProc(hwnd, msg, wp, lp);
}

// ---- WinMain -----------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Single-instance guard
    HANDLE mutex = CreateMutexW(nullptr, TRUE, APP_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Register invisible message-only window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProcDispatch;
    wc.hInstance     = hInst;
    wc.lpszClassName = APP_NAME;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY));
    RegisterClassExW(&wc);

    // HWND_MESSAGE = message-only window (no taskbar button, no visible window)
    g_hwnd = CreateWindowExW(0, APP_NAME, APP_NAME,
                              0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    Config_Load(g_app.cfg);

    // Apply autostart setting from config
    Config_SetAutostart(g_app.cfg.startWithWindows);

    if (!Tray_Init(g_hwnd)) {
        DestroyWindow(g_hwnd);
        return 1;
    }

    UpdateTray();

    // Message loop
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mutex) CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}
