#include "history.h"
#include <strsafe.h>

// RAM history ring buffer
static int s_samples[HISTORY_SAMPLES]{};
static int s_head = 0;
static int s_count = 0;

void History_Push(int usedPercent) {
    s_samples[s_head] = usedPercent;
    s_head = (s_head + 1) % HISTORY_SAMPLES;
    if (s_count < HISTORY_SAMPLES) ++s_count;
}

void History_GetSamples(int* out, int count) {
    if (count > s_count) count = s_count;
    int start = (s_head - count + HISTORY_SAMPLES) % HISTORY_SAMPLES;
    for (int i = 0; i < count; ++i)
        out[i] = s_samples[(start + i) % HISTORY_SAMPLES];
}

int History_Count() { return s_count; }

// Clean event ring buffer — last 50 cleans in memory
#define CLEAN_LOG_MAX 50
static CleanEvent s_events[CLEAN_LOG_MAX]{};
static int s_evHead = 0;
static int s_evCount = 0;

void History_LogClean(double freedMB, int processCount) {
    CleanEvent& e = s_events[s_evHead];
    GetLocalTime(&e.time);
    e.freedMB = freedMB;
    e.processCount = processCount;
    s_evHead = (s_evHead + 1) % CLEAN_LOG_MAX;
    if (s_evCount < CLEAN_LOG_MAX) ++s_evCount;
}

int History_GetRecentCleans(CleanEvent* out, int maxCount) {
    if (maxCount > s_evCount) maxCount = s_evCount;
    int start = (s_evHead - maxCount + CLEAN_LOG_MAX) % CLEAN_LOG_MAX;
    for (int i = 0; i < maxCount; ++i)
        out[i] = s_events[(start + i) % CLEAN_LOG_MAX];
    return maxCount;
}

bool History_WriteCleanLog(const wchar_t* path, double freedMB, int processCount) {
    HANDLE hFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[256]{};
    StringCchPrintfW(line, 256,
        L"%04d-%02d-%02d %02d:%02d:%02d  Freed %.0f MB  (%d processes)\r\n",
        t.wYear, t.wMonth, t.wDay,
        t.wHour, t.wMinute, t.wSecond,
        freedMB, processCount);

    char utf8[512]{};
    WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, 512, nullptr, nullptr);
    DWORD written;
    WriteFile(hFile, utf8, (DWORD)strlen(utf8), &written, nullptr);
    CloseHandle(hFile);
    return true;
}
