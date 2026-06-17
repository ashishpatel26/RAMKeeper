#include "cleaner.h"
#include <strsafe.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(st) ((NTSTATUS)(st) >= 0)
#endif

typedef NTSTATUS(WINAPI* PFN_NtSetSystemInfo)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG);

static constexpr SYSTEM_INFORMATION_CLASS SystemMemoryListInformation =
    static_cast<SYSTEM_INFORMATION_CLASS>(0x50);

enum SYSTEM_MEMORY_LIST_COMMAND : DWORD {
    MemoryFlushModifiedList = 3,
    MemoryPurgeStandbyList  = 4,
};

static PFN_NtSetSystemInfo s_NtSetSystemInfo = nullptr;

static void LoadNtdll() {
    if (s_NtSetSystemInfo) return;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll)
        s_NtSetSystemInfo = reinterpret_cast<PFN_NtSetSystemInfo>(
            GetProcAddress(ntdll, "NtSetSystemInformation"));
}

static bool PurgeStandbyList() {
    LoadNtdll();
    if (!s_NtSetSystemInfo) return false;
    SYSTEM_MEMORY_LIST_COMMAND cmd = MemoryPurgeStandbyList;
    return NT_SUCCESS(s_NtSetSystemInfo(SystemMemoryListInformation, &cmd, sizeof(cmd)));
}

static bool FlushModifiedList() {
    LoadNtdll();
    if (!s_NtSetSystemInfo) return false;
    SYSTEM_MEMORY_LIST_COMMAND cmd = MemoryFlushModifiedList;
    return NT_SUCCESS(s_NtSetSystemInfo(SystemMemoryListInformation, &cmd, sizeof(cmd)));
}

static bool ClearFileSystemCache() {
    return SetSystemFileCacheSize(
        static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1), 0) != FALSE;
}

// ponytail: O(n*k) walk — k is typically 0-5 exclusion entries, not worth a hashset
static bool IsExcluded(HANDLE hProc, const wchar_t* excludeList) {
    wchar_t img[MAX_PATH]{};
    if (!GetProcessImageFileNameW(hProc, img, MAX_PATH)) return false;

    const wchar_t* fname = wcsrchr(img, L'\\');
    fname = fname ? fname + 1 : img;

    // Use mutable copy for wcstok_s
    wchar_t copy[256]{};
    StringCchCopyW(copy, 256, excludeList);

    wchar_t* ctx = nullptr;
    wchar_t* tok = wcstok_s(copy, L", ", &ctx);
    while (tok) {
        if (_wcsnicmp(fname, tok, 256) == 0) return true;
        tok = wcstok_s(nullptr, L", ", &ctx);
    }
    return false;
}

int Cleaner_TrimWorkingSets(const wchar_t* excludeList) {
    DWORD pids[4096]{};  // ponytail: fixed cap — >4096 procs is extremely rare
    DWORD bytesReturned = 0;
    if (!EnumProcesses(pids, sizeof(pids), &bytesReturned))
        return 0;

    bool hasExclude = excludeList && excludeList[0];
    int count = 0;
    DWORD numPids = bytesReturned / sizeof(DWORD);

    for (DWORD i = 0; i < numPids; ++i) {
        if (pids[i] == 0) continue;

        HANDLE hProc = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pids[i]);
        if (!hProc) continue;

        if (!hasExclude || !IsExcluded(hProc, excludeList)) {
            if (EmptyWorkingSet(hProc)) ++count;
        }

        CloseHandle(hProc);
    }
    return count;
}

int Cleaner_Run(const wchar_t* excludeList) {
    int trimmed = Cleaner_TrimWorkingSets(excludeList);
    FlushModifiedList();
    PurgeStandbyList();
    ClearFileSystemCache();
    return trimmed;
}
