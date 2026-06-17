#pragma once

// Target Windows 7+ (0x0601)
#define WINVER       0x0601
#define _WIN32_WINNT 0x0601
#define _WIN32_IE    0x0700

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <commctrl.h>
#include <winternl.h>

// WM_ IDs used across modules
#define WM_TRAYICON   (WM_USER + 1)
#define WM_CLEANRAM   (WM_USER + 2)

// Timer ID
#define TIMER_POLL    1

// Tray icon ID
#define IDI_TRAY      1

// Menu item IDs
#define IDM_CLEAN_NOW     100
#define IDM_AUTO_TOGGLE   101
#define IDM_SETTINGS      102
#define IDM_EXIT          103
#define IDM_STATUS        104

// Global hotkey ID
#define HOTKEY_CLEAN  1

// App name (used for window class, mutex, config dir)
#define APP_NAME      L"RAMKeeper"
#define APP_MUTEX     L"RAMKeeper_SingleInstance"
