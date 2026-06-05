#include "settings.h"
#include <strsafe.h>
#include <stdlib.h>

// ── Control IDs ────────────────────────────────────────────────────────────
#define IDC_EDT_THRESHOLD   200
#define IDC_EDT_INTERVAL    201
#define IDC_EDT_IDLE        202
#define IDC_EDT_BOOT        203
#define IDC_CHK_AUTOCLEAN   204
#define IDC_CHK_SILENT      205
#define IDC_CHK_AUTOSTART   206
#define IDC_BTN_OK          207
#define IDC_BTN_CANCEL      208

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

// ── Set dialog font to system UI font ───────────────────────────────────────
static void SetChildFont(HWND hwnd, HFONT hfont) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, (WPARAM)lp, TRUE);
        return TRUE;
    }, (LPARAM)hfont);
}

// ── Window procedure ─────────────────────────────────────────────────────────
static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        // Store state pointer passed via lpCreateParams
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

        int lx = 14, ex = 220, ew = 70, lw = 200;

        // Section: Auto-clean
        MakeLabel(hwnd, L"Auto-clean Triggers", lx, 14, 300, 18);
        MakeLabel(hwnd, L"RAM threshold (%):",       lx, 40,  lw, 18);
        MakeEdit (hwnd, IDC_EDT_THRESHOLD, ex, 38, ew, 22, st->cfg->thresholdPercent);
        MakeLabel(hwnd, L"Clean interval (min, 0=off):", lx, 68,  lw, 18);
        MakeEdit (hwnd, IDC_EDT_INTERVAL,  ex, 66, ew, 22, st->cfg->intervalMinutes);
        MakeLabel(hwnd, L"Idle timeout (min, 0=off):", lx, 96,  lw, 18);
        MakeEdit (hwnd, IDC_EDT_IDLE,      ex, 94, ew, 22, st->cfg->onIdleMinutes);
        MakeLabel(hwnd, L"Boot delay (sec, 0=off):",   lx, 124, lw, 18);
        MakeEdit (hwnd, IDC_EDT_BOOT,      ex, 122, ew, 22, st->cfg->onBootDelaySec);

        // Separator line (static with SS_ETCHEDHORZ)
        CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            lx, 154, 310, 2, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

        // Section: Options
        MakeLabel(hwnd, L"Options", lx, 162, 300, 18);
        MakeCheck(hwnd, IDC_CHK_AUTOCLEAN, L"Enable auto-clean",           lx, 184, 290, st->cfg->autoClean);
        MakeCheck(hwnd, IDC_CHK_SILENT,    L"Silent mode (no balloons)",   lx, 208, 290, st->cfg->silentMode);
        MakeCheck(hwnd, IDC_CHK_AUTOSTART, L"Start with Windows",          lx, 232, 290, st->cfg->startWithWindows);

        // OK / Cancel
        MakeBtn(hwnd, IDC_BTN_OK,     L"OK",     174, 270, 76, true);
        MakeBtn(hwnd, IDC_BTN_CANCEL, L"Cancel", 256, 270, 76, false);

        // Apply Segoe UI 9pt to all children
        HFONT hf = CreateFontW(-MulDiv(9, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72),
            0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SetChildFont(hwnd, hf);
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hf, TRUE);

        // Bold section labels
        HFONT hfBold = CreateFontW(-MulDiv(9, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72),
            0,0,0, FW_BOLD, 0,0,0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SendMessageW(GetDlgItem(hwnd, 0), WM_SETFONT, (WPARAM)hfBold, TRUE);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_OK: {
            int thresh = GetEditInt(hwnd, IDC_EDT_THRESHOLD, 1, 99);
            int interval= GetEditInt(hwnd, IDC_EDT_INTERVAL, 0, 1440);
            int idle    = GetEditInt(hwnd, IDC_EDT_IDLE,     0, 1440);
            int boot    = GetEditInt(hwnd, IDC_EDT_BOOT,     0, 3600);

            st->cfg->thresholdPercent = thresh;
            st->cfg->intervalMinutes  = interval;
            st->cfg->onIdleMinutes    = idle;
            st->cfg->onBootDelaySec   = boot;
            st->cfg->autoClean        = GetCheck(hwnd, IDC_CHK_AUTOCLEAN);
            st->cfg->silentMode       = GetCheck(hwnd, IDC_CHK_SILENT);
            st->cfg->startWithWindows = GetCheck(hwnd, IDC_CHK_AUTOSTART);

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
        PostQuitMessage(0); // unblock the nested message loop
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
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
        wc.hIcon         = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    DlgState state{ &cfg, false };

    // Center on screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 350, wh = 318;
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

    // Disable parent (tray message-only window can't be disabled, but good practice)
    if (parent && IsWindowVisible(parent))
        EnableWindow(parent, FALSE);

    // Nested message loop — blocks until DestroyWindow posts WM_QUIT
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (parent && IsWindowVisible(parent))
        EnableWindow(parent, TRUE);

    return state.accepted;
}
