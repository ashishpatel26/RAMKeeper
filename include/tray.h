#pragma once
#include "common.h"

bool Tray_Init(HWND hwnd);
void Tray_Update(HWND hwnd, double usedGB, double totalGB, int pct);
void Tray_Destroy(HWND hwnd);
void Tray_ShowContextMenu(HWND hwnd, bool autoCleanEnabled);
