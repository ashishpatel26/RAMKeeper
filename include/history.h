#pragma once
#include "common.h"

#define HISTORY_SAMPLES 300   // 5 min at 1/sec

struct CleanEvent {
    SYSTEMTIME time;
    double     freedMB;
    int        processCount;
};

void History_Push(int usedPercent);
void History_GetSamples(int* out, int count); // oldest-first, count <= HISTORY_SAMPLES
int  History_Count();

void History_LogClean(double freedMB, int processCount);
int  History_GetRecentCleans(CleanEvent* out, int maxCount);

bool History_WriteCleanLog(const wchar_t* path, double freedMB, int processCount);
