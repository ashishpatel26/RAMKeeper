#include "status.h"
#include "history.h"
#include "stats.h"
#include <strsafe.h>
#include <dwmapi.h>

static HWND  s_hwnd    = nullptr;
static HFONT s_fontLg  = nullptr;  // large RAM % number
static HFONT s_fontSm  = nullptr;  // body text

#define STATUS_W  460
#define STATUS_H  340

// Dark theme palette
#define CLR_BG       RGB(20,  20,  28)
#define CLR_CARD     RGB(32,  32,  44)
#define CLR_TEXT     RGB(230, 230, 240)
#define CLR_DIM      RGB(130, 130, 155)
#define CLR_BORDER   RGB(55,  55,  75)
#define CLR_GREEN    RGB(50,  210, 80)
#define CLR_YELLOW   RGB(255, 195, 0)
#define CLR_RED      RGB(225, 60,  60)
#define CLR_GRAPH    RGB(0,   130, 220)

static COLORREF RamColor(int pct) {
    return (pct >= 80) ? CLR_RED : (pct >= 60) ? CLR_YELLOW : CLR_GREEN;
}

static bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION e{};
    DWORD sz;
    bool r = GetTokenInformation(tok, TokenElevation, &e, sizeof(e), &sz) && e.TokenIsElevated;
    CloseHandle(tok);
    return r;
}

static void DrawGraph(HDC hdc, RECT rc) {
    HBRUSH bg = CreateSolidBrush(CLR_CARD);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    // Border
    HPEN border = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, border);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    int n = History_Count();
    if (n < 2) return;

    // ponytail: stack-alloc up to HISTORY_SAMPLES ints — 300*4 = 1.2KB, fine on stack
    int samples[HISTORY_SAMPLES];
    History_GetSamples(samples, n);

    int gw = rc.right  - rc.left - 2;
    int gh = rc.bottom - rc.top  - 2;
    int ox = rc.left + 1;
    int oy = rc.top  + 1;

    // Fill area under graph
    POINT pts[HISTORY_SAMPLES + 2];
    int display = (n < gw) ? n : gw;
    int start = n - display;
    for (int i = 0; i < display; ++i) {
        pts[i].x = ox + i * gw / display;
        pts[i].y = oy + gh - samples[start + i] * gh / 100;
    }
    pts[display].x   = ox + gw;
    pts[display].y   = oy + gh;
    pts[display + 1].x = ox;
    pts[display + 1].y = oy + gh;

    HBRUSH fill = CreateSolidBrush(RGB(0, 70, 130));
    HBRUSH oldFill = (HBRUSH)SelectObject(hdc, fill);
    HPEN noPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, pts, display + 2);
    SelectObject(hdc, oldFill);
    SelectObject(hdc, noPen);
    DeleteObject(fill);

    // Line on top
    HPEN linePen = CreatePen(PS_SOLID, 1, CLR_GRAPH);
    oldPen = (HPEN)SelectObject(hdc, linePen);
    MoveToEx(hdc, pts[0].x, pts[0].y, nullptr);
    for (int i = 1; i < display; ++i)
        LineTo(hdc, pts[i].x, pts[i].y);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

static LRESULT CALLBACK StatusProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // Dark title bar (Win10 1809+ attribute 20; no-op on older)
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        // Win11 rounded corners (attribute 33, DWMWCP_ROUND = 2)
        DWORD corner = 2;
        DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));

        // Build fonts once
        LOGFONTW lf{};
        SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
        s_fontSm = CreateFontIndirectW(&lf);
        lf.lfHeight = lf.lfHeight * 4;
        lf.lfWeight = FW_BOLD;
        s_fontLg = CreateFontIndirectW(&lf);

        SetTimer(hwnd, 2, 1000, nullptr);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 2);
        if (s_fontLg) { DeleteObject(s_fontLg); s_fontLg = nullptr; }
        if (s_fontSm) { DeleteObject(s_fontSm); s_fontSm = nullptr; }
        s_hwnd = nullptr;
        return 0;

    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT cr;
        GetClientRect(hwnd, &cr);

        // Double-buffer to eliminate flicker
        HDC memDC  = CreateCompatibleDC(hdc);
        HBITMAP bmp    = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

        // Background
        HBRUSH bgBr = CreateSolidBrush(CLR_BG);
        FillRect(memDC, &cr, bgBr);
        DeleteObject(bgBr);

        SetBkMode(memDC, TRANSPARENT);

        RamStats s{};
        Stats_Get(s);

        // Large RAM % — top-left
        SelectObject(memDC, s_fontLg);
        SetTextColor(memDC, RamColor(s.usedPercent));
        wchar_t buf[64]{};
        StringCchPrintfW(buf, 64, L"%d%%", s.usedPercent);
        TextOutW(memDC, 20, 18, buf, (int)wcslen(buf));

        // GB subtitle
        SelectObject(memDC, s_fontSm);
        SetTextColor(memDC, CLR_DIM);
        StringCchPrintfW(buf, 64, L"%.1f / %.1f GB used", s.usedGB, s.totalGB);
        TextOutW(memDC, 20, 76, buf, (int)wcslen(buf));

        // Admin badge — top right
        static int isAdmin = -1;
        if (isAdmin < 0) isAdmin = IsElevated() ? 1 : 0;
        SetTextColor(memDC, isAdmin ? CLR_GREEN : CLR_DIM);
        const wchar_t* badge = isAdmin ? L"Admin ✓" : L"Limited";
        SIZE sz{};
        GetTextExtentPoint32W(memDC, badge, (int)wcslen(badge), &sz);
        TextOutW(memDC, cr.right - sz.cx - 16, 22, badge, (int)wcslen(badge));

        // Graph
        RECT grc = {20, 108, cr.right - 20, 208};
        DrawGraph(memDC, grc);

        // Graph caption
        SetTextColor(memDC, CLR_DIM);
        TextOutW(memDC, 20, 213, L"5 min", 5);

        // Divider
        HPEN div = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN odiv = (HPEN)SelectObject(memDC, div);
        MoveToEx(memDC, 20, 234, nullptr);
        LineTo(memDC, cr.right - 20, 234);
        SelectObject(memDC, odiv);
        DeleteObject(div);

        // Recent cleans heading
        SetTextColor(memDC, CLR_TEXT);
        TextOutW(memDC, 20, 242, L"Recent cleans", 13);

        CleanEvent events[5]{};
        int n = History_GetRecentCleans(events, 5);
        if (n == 0) {
            SetTextColor(memDC, CLR_DIM);
            TextOutW(memDC, 20, 262, L"No cleans yet this session", 26);
        } else {
            for (int i = n - 1; i >= 0; --i) {
                SetTextColor(memDC, CLR_DIM);
                StringCchPrintfW(buf, 64, L"  %02d:%02d  Freed %.0f MB  (%d procs)",
                    events[i].time.wHour, events[i].time.wMinute,
                    events[i].freedMB, events[i].processCount);
                TextOutW(memDC, 20, 262 + (n - 1 - i) * 17,
                         buf, (int)wcslen(buf));
            }
        }

        BitBlt(hdc, 0, 0, cr.right, cr.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;

    case WM_ACTIVATE:
        // Destroy on lose focus so it behaves like a popup
        if (LOWORD(wp) == WA_INACTIVE) DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool s_classReg = false;

void Status_Show(HWND parent) {
    if (s_hwnd) {
        SetForegroundWindow(s_hwnd);
        return;
    }

    if (!s_classReg) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = StatusProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.lpszClassName = L"RAMKeeperStatus";
        RegisterClassExW(&wc);
        s_classReg = true;
    }

    // Position above tray (bottom-right area)
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.right  - STATUS_W - 12;
    int y = workArea.bottom - STATUS_H - 12;

    s_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"RAMKeeperStatus", L"RAMKeeper",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, STATUS_W, STATUS_H,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr);

    // WS_EX_NOACTIVATE prevents stealing focus; but we need to handle
    // activation ourselves to dismiss on click-away via WM_ACTIVATE
    // Re-enable normal activation after show
    SetWindowLongW(s_hwnd, GWL_EXSTYLE,
        GetWindowLongW(s_hwnd, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);
    SetForegroundWindow(s_hwnd);
    (void)sw; (void)sh;
}

void Status_Hide() {
    if (s_hwnd) DestroyWindow(s_hwnd);
}

bool Status_IsVisible() { return s_hwnd != nullptr; }

void Status_Update() {
    if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE);
}
