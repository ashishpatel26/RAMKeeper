#include "settings.h"
#include <strsafe.h>
#include <stdlib.h>

// ── Control IDs ────────────────────────────────────────────────────────────
#define IDC_EDT_THRESHOLD    200
#define IDC_EDT_INTERVAL     201
#define IDC_EDT_IDLE         202
#define IDC_EDT_BOOT         203
#define IDC_CHK_AUTOCLEAN    204
#define IDC_CHK_SILENT       205
#define IDC_CHK_AUTOSTART    206
#define IDC_BTN_OK           207
#define IDC_BTN_CANCEL       208
// v1.1 additions
#define IDC_EDT_CLEAN_HOUR   209
#define IDC_EDT_CLEAN_MIN    210
#define IDC_EDT_NOTIFY_MB    211
#define IDC_EDT_EXCLUDE      212
#define IDC_CHK_SHOW_STATUS  213

// ── Dialog state ────────────────────────────────────────────────────────────
struct DlgState {
    AppConfig* cfg;
    bool       accepted;
};

// ── Helpers ─────────────────────────────────────────────────────────────────
static HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

static HWND MakeEdit(HWND parent, int id, int x, int y, int w, int h, int val) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
        x, y, w, h,
        parent, reinterpret_cast<HMENU>((UINT_PTR)id),
        GetModuleHandleW(nullptr), nullptr);
    wchar_t buf[16]{};
    StringCchPrintfW(buf, 16, L"%d", val);
    SetWindowTextW(hw, buf);
    return hw;
}

static HWND MakeTextEdit(HWND parent, int id, int x, int y, int w, int h, const wchar_t* text) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
        x, y, w, h,
        parent, reinterpret_cast<HMENU>((UINT_PTR)id),
        GetModuleHandleW(nullptr), nullptr);
    return hw;
}

static HWND MakeCheck(HWND parent, int id, const wchar_t* text, int x, int y, int w, bool checked) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, w, 20,
        parent, reinterpret_cast<HMENU>((UINT_PTR)id),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hw, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return hw;
}

static HWND MakeBtn(HWND parent, int id, const wchar_t* text, int x, int y, int w, bool defBtn) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    if (defBtn) style |= BS_DEFPUSHBUTTON;
    return CreateWindowExW(0, L"BUTTON", text, style,
        x, y, w, 28,
        parent, reinterpret_cast<HMENU>((UINT_PTR)id),
        GetModuleHandleW(nullptr), nullptr);
}

static int GetEditInt(HWND parent, int id, int minVal = 0, int maxVal = 9999) {
    wchar_t buf[16]{};
    GetDlgItemTextW(parent, id, buf, 16);
    int v = _wtoi(buf);
    if (v < minVal) v = minVal;
    if (v > maxVal) v = maxVal;
    return v;
}

static bool GetCheck(HWND parent, int id) {
    return SendDlgItemMessageW(parent, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void SetChildFont(HWND hwnd, HFONT hfont) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, (WPARAM)lp, TRUE);
        return TRUE;
    }, (LPARAM)hfont);
}

static HFONT MakeFont(HWND hwnd, int ptSz, bool bold) {
    int h = -MulDiv(ptSz, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72);
    return CreateFontW(h, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void HSep(HWND parent, int y, int lx, int w) {
    CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        lx, y, w, 2, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

// ── Window procedure ─────────────────────────────────────────────────────────
static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

        const int lx = 14, ex = 220, ew = 70, lw = 200, sep_w = 314;

        // ── Auto-clean Triggers ──
        MakeLabel(hwnd, L"Auto-clean Triggers",           lx, 14,  300, 18);
        MakeLabel(hwnd, L"RAM threshold (%):",            lx, 40,  lw,  18);
        MakeEdit (hwnd, IDC_EDT_THRESHOLD, ex, 38, ew, 22, st->cfg->thresholdPercent);
        MakeLabel(hwnd, L"Clean interval (min, 0=off):",  lx, 68,  lw,  18);
        MakeEdit (hwnd, IDC_EDT_INTERVAL,  ex, 66, ew, 22, st->cfg->intervalMinutes);
        MakeLabel(hwnd, L"Idle timeout (min, 0=off):",    lx, 96,  lw,  18);
        MakeEdit (hwnd, IDC_EDT_IDLE,      ex, 94, ew, 22, st->cfg->onIdleMinutes);
        MakeLabel(hwnd, L"Boot delay (sec, 0=off):",      lx, 124, lw,  18);
        MakeEdit (hwnd, IDC_EDT_BOOT,      ex, 122, ew, 22, st->cfg->onBootDelaySec);

        HSep(hwnd, 154, lx, sep_w);

        // ── Options ──
        MakeLabel(hwnd, L"Options", lx, 162, 300, 18);
        MakeCheck(hwnd, IDC_CHK_AUTOCLEAN, L"Enable auto-clean",         lx, 184, 290, st->cfg->autoClean);
        MakeCheck(hwnd, IDC_CHK_SILENT,    L"Silent mode (no balloons)", lx, 208, 290, st->cfg->silentMode);
        MakeCheck(hwnd, IDC_CHK_AUTOSTART, L"Start with Windows",        lx, 232, 290, st->cfg->startWithWindows);

        HSep(hwnd, 260, lx, sep_w);

        // ── Schedule & Notify ──
        MakeLabel(hwnd, L"Schedule & Notifications", lx, 268, 300, 18);
        MakeLabel(hwnd, L"Scheduled clean hour (0-23, -1=off):", lx, 290, 200, 18);
        MakeEdit (hwnd, IDC_EDT_CLEAN_HOUR, ex, 288, ew, 22, st->cfg->cleanHour);
        MakeLabel(hwnd, L"Scheduled clean minute (0-59):", lx, 314, 200, 18);
        MakeEdit (hwnd, IDC_EDT_CLEAN_MIN,  ex, 312, ew, 22, st->cfg->cleanMinute);
        MakeLabel(hwnd, L"Min freed MB to notify:",        lx, 338, 200, 18);
        MakeEdit (hwnd, IDC_EDT_NOTIFY_MB,  ex, 336, ew, 22, st->cfg->notifyMinMB);
        MakeCheck(hwnd, IDC_CHK_SHOW_STATUS, L"Open status window after each clean",
                  lx, 362, 290, st->cfg->showStatusOnClean);

        HSep(hwnd, 390, lx, sep_w);

        // ── Process Exclusion ──
        MakeLabel(hwnd, L"Process Exclusion", lx, 398, 300, 18);
        MakeLabel(hwnd, L"Exclude from working-set trim (comma-sep exe names):",
                  lx, 416, 310, 18);
        MakeTextEdit(hwnd, IDC_EDT_EXCLUDE, lx, 436, 314, 22, st->cfg->excludeList);

        // ── OK / Cancel ──
        MakeBtn(hwnd, IDC_BTN_OK,     L"OK",     174, 472, 76, true);
        MakeBtn(hwnd, IDC_BTN_CANCEL, L"Cancel", 256, 472, 76, false);

        // Apply Segoe UI 9pt to all children
        HFONT hf     = MakeFont(hwnd, 9, false);
        HFONT hfBold = MakeFont(hwnd, 9, true);
        SetChildFont(hwnd, hf);
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hf, TRUE);

        // Section headers bold — first child of each section via GetDlgItem is tricky;
        // use EnumChildWindows trick skipped here; section labels use static IDs 0,
        // so we apply bold globally and rely on Segoe UI weight
        (void)hfBold; // ponytail: bold headers require unique IDs; not worth the plumbing
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_OK: {
            st->cfg->thresholdPercent = GetEditInt(hwnd, IDC_EDT_THRESHOLD, 1, 99);
            st->cfg->intervalMinutes  = GetEditInt(hwnd, IDC_EDT_INTERVAL,  0, 1440);
            st->cfg->onIdleMinutes    = GetEditInt(hwnd, IDC_EDT_IDLE,      0, 1440);
            st->cfg->onBootDelaySec   = GetEditInt(hwnd, IDC_EDT_BOOT,      0, 3600);
            st->cfg->autoClean        = GetCheck(hwnd, IDC_CHK_AUTOCLEAN);
            st->cfg->silentMode       = GetCheck(hwnd, IDC_CHK_SILENT);
            st->cfg->startWithWindows = GetCheck(hwnd, IDC_CHK_AUTOSTART);

            // v1.1 fields
            st->cfg->cleanHour        = GetEditInt(hwnd, IDC_EDT_CLEAN_HOUR, -1, 23);
            st->cfg->cleanMinute      = GetEditInt(hwnd, IDC_EDT_CLEAN_MIN,   0, 59);
            st->cfg->notifyMinMB      = GetEditInt(hwnd, IDC_EDT_NOTIFY_MB,   0, 9999);
            st->cfg->showStatusOnClean = GetCheck(hwnd, IDC_CHK_SHOW_STATUS);

            GetDlgItemTextW(hwnd, IDC_EDT_EXCLUDE,
                st->cfg->excludeList, 256);

            Config_Save(*st->cfg);
            Config_SetAutostart(st->cfg->startWithWindows);
            st->accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDC_BTN_CANCEL:
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Public API ───────────────────────────────────────────────────────────────
bool Settings_Show(HWND parent, AppConfig& cfg) {
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = SettingsProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"RAMKeeperSettings";
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hIcon         = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    DlgState state{ &cfg, false };

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 350, wh = 520;
    int wx = (sw - ww) / 2, wy = (sh - wh) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"RAMKeeperSettings",
        L"RAMKeeper — Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        wx, wy, ww, wh,
        parent, nullptr, GetModuleHandleW(nullptr), &state);

    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (parent && IsWindowVisible(parent))
        EnableWindow(parent, FALSE);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (msg.message == WM_QUIT) PostQuitMessage(static_cast<int>(msg.wParam));

    if (parent && IsWindowVisible(parent))
        EnableWindow(parent, TRUE);

    return state.accepted;
}
