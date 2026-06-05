#pragma once
#include "common.h"
#include "config.h"

// Shows modal settings dialog. Saves to cfg on OK.
// Returns true if user clicked OK.
bool Settings_Show(HWND parent, AppConfig& cfg);
