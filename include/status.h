#pragma once
#include "common.h"

void Status_Show(HWND parent);
void Status_Hide();
bool Status_IsVisible();
void Status_Update(); // call from OnTimer to refresh display
