#pragma once
#include "common.h"

// Full RAM clean sequence (trimWorkingSets + NT ops + file cache).
// excludeList: comma-separated exe names to skip from working-set trim (nullptr = trim all).
// Returns number of processes trimmed.
int Cleaner_Run(const wchar_t* excludeList = nullptr);

int Cleaner_TrimWorkingSets(const wchar_t* excludeList = nullptr);
