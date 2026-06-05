#pragma once
#include "common.h"

// Perform full RAM clean sequence:
//   1. EmptyWorkingSet on all accessible processes
//   2. NtSetSystemInformation(MemoryPurgeStandbyList)
//   3. NtSetSystemInformation(MemoryFlushModifiedList)
//   4. SetSystemFileCacheSize(-1,-1,0)
// Returns number of processes trimmed.
int Cleaner_Run();

// Trim only working sets (no admin required, degraded mode)
int Cleaner_TrimWorkingSets();
