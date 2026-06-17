#include "tray.h"
#include <strsafe.h>

static NOTIFYICONDATAW s_nid{};
static HICON s_colorIcon = nullptr; // dynamically drawn color icon

static HICON CreateRamIcon(int pct) {
    int sz = GetSystemMetrics(SM_CXSMICON);
    if (sz <= 0) sz = 16;

    HDC screenDC = GetDC(nullptr);
    HDC memDC    = CreateCompatibleDC(screenDC);

    // 32bpp DIB so we get clean alpha on modern Windows
    BITMAPV5HEADER bh{};
    bh.bV5Size       = sizeof(bh);
    bh.bV5Width      = sz;
    bh.bV5Height     = -sz; // top-down
    bh.bV5Planes     = 1;
    bh.bV5BitCount   = 32;
    bh.bV5Compression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp    = CreateDIBSection(screenDC, (BITMAPINFO*)&bh,
                          DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBmp  = (HBITMAP)SelectObject(memDC, hBmp);

    // Dark background
    RECT rc = {0, 0, sz, sz};
    HBRUSH bgBr = CreateSolidBrush(RGB(28, 28, 35));
    FillRect(memDC, &rc, bgBr);
    DeleteObject(bgBr);

    // Vertical bar fill — green/yellow/red
    COLORREF c = (pct >= 80) ? RGB(220, 55, 55)
               : (pct >= 60) ? RGB(220, 175, 0)
               :               RGB(45, 200, 75);
    int barH = sz * pct / 100;
    RECT barRc = {1, sz - barH, sz - 1, sz};
    HBRUSH barBr = CreateSolidBrush(c);
    FillRect(memDC, &barRc, barBr);
    DeleteObject(barBr);

    // Opaque mask (all zeros = color bitmap fully visible)
    HBITMAP hMask = CreateBitmap(sz, sz, 1, 1, nullptr);

    ICONINFO ii{ TRUE, 0, 0, hMask, hBmp };
    HICON icon = CreateIconIndirect(&ii);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteObject(hMask);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return icon;
}

bool Tray_Init(HWND hwnd) {
    s_nid        = {};
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd   = hwnd;
    s_nid.uID    = IDI_TRAY;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_TRAYICON;

    s_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr),
                            MAKEINTRESOURCEW(IDI_TRAY));
    if (!s_nid.hIcon)
        s_nid.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));

    StringCchCopyW(s_nid.szTip, ARRAYSIZE(s_nid.szTip), APP_NAME);
    return Shell_NotifyIconW(NIM_ADD, &s_nid) != FALSE;
}

void Tray_Update(HWND hwnd, double usedGB, double totalGB, int pct) {
    (void)hwnd;
    s_nid.uFlags = NIF_ICON | NIF_TIP;

    wchar_t tip[128]{};
    StringCchPrintfW(tip, ARRAYSIZE(tip),
        L"%s\n%.1f / %.1f GB  (%d%%)",
        APP_NAME, usedGB, totalGB, pct);
    StringCchCopyW(s_nid.szTip, ARRAYSIZE(s_nid.szTip), tip);

    // Color-coded icon
    HICON prev = s_colorIcon;
    s_colorIcon  = CreateRamIcon(pct);
    s_nid.hIcon  = s_colorIcon;
    Shell_NotifyIconW(NIM_MODIFY, &s_nid);
    if (prev) DestroyIcon(prev);
}

void Tray_Destroy(HWND hwnd) {
    (void)hwnd;
    Shell_NotifyIconW(NIM_DELETE, &s_nid);
    if (s_colorIcon) { DestroyIcon(s_colorIcon); s_colorIcon = nullptr; }
}

void Tray_ShowContextMenu(HWND hwnd, bool autoCleanEnabled) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    MENUITEMINFOW mii{};
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING | MIIM_ID | MIIM_STATE;
    mii.wID        = IDM_CLEAN_NOW;
    mii.fState     = MFS_DEFAULT;
    mii.dwTypeData = const_cast<LPWSTR>(L"Clean Now\tCtrl+Alt+R");
    InsertMenuItemW(menu, 0, TRUE, &mii);

    AppendMenuW(menu, MF_STRING, IDM_STATUS, L"Status Window...");
    AppendMenuW(menu, MF_STRING | (autoCleanEnabled ? MF_CHECKED : MF_UNCHECKED),
                IDM_AUTO_TOGGLE, L"Auto-clean");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    POINT pt{};
    GetCursorPos(&pt);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                   pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}
