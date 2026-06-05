#include "tray.h"
#include <strsafe.h>

static NOTIFYICONDATAW s_nid{};

bool Tray_Init(HWND hwnd) {
    s_nid         = {};
    s_nid.cbSize  = sizeof(s_nid);
    s_nid.hWnd    = hwnd;
    s_nid.uID     = IDI_TRAY;
    s_nid.uFlags  = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_TRAYICON;

    // Load icon from resources (resource ID 1)
    s_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr),
                            MAKEINTRESOURCEW(IDI_TRAY));
    if (!s_nid.hIcon)
        s_nid.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); // IDI_APPLICATION fallback

    StringCchCopyW(s_nid.szTip, ARRAYSIZE(s_nid.szTip), APP_NAME);

    return Shell_NotifyIconW(NIM_ADD, &s_nid) != FALSE;
}

void Tray_Update(HWND hwnd, double usedGB, double totalGB, int pct) {
    (void)hwnd;
    s_nid.uFlags = NIF_ICON | NIF_TIP;

    // Format: "RAMKeeper\n8.3 / 16.0 GB (52%)"
    wchar_t tip[128]{};
    StringCchPrintfW(tip, ARRAYSIZE(tip),
        L"%s\n%.1f / %.1f GB  (%d%%)",
        APP_NAME, usedGB, totalGB, pct);
    StringCchCopyW(s_nid.szTip, ARRAYSIZE(s_nid.szTip), tip);

    Shell_NotifyIconW(NIM_MODIFY, &s_nid);
}

void Tray_Destroy(HWND hwnd) {
    (void)hwnd;
    Shell_NotifyIconW(NIM_DELETE, &s_nid);
    if (s_nid.hIcon)
        DestroyIcon(s_nid.hIcon);
}

void Tray_ShowContextMenu(HWND hwnd, bool autoCleanEnabled) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    // Bold "Clean Now" as default item
    MENUITEMINFOW mii{};
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING | MIIM_ID | MIIM_STATE;
    mii.wID        = IDM_CLEAN_NOW;
    mii.fState     = MFS_DEFAULT; // bold
    mii.dwTypeData = const_cast<LPWSTR>(L"Clean Now\tCtrl+Alt+R");
    InsertMenuItemW(menu, 0, TRUE, &mii);

    // Auto-clean toggle (checked when enabled)
    AppendMenuW(menu, MF_STRING | (autoCleanEnabled ? MF_CHECKED : MF_UNCHECKED),
                IDM_AUTO_TOGGLE, L"Auto-clean");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT,     L"Exit");

    // Required: foreground window so menu dismisses on click-away
    SetForegroundWindow(hwnd);

    POINT pt{};
    GetCursorPos(&pt);

    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                   pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(menu);
}
