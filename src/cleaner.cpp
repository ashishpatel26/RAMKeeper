#include "cleaner.h"

// NT_SUCCESS is in ntstatus.h which conflicts with windows.h — define directly
#ifndef NT_SUCCESS
#define NT_SUCCESS(st) ((NTSTATUS)(st) >= 0)
#endif

// NtSetSystemInformation is documented but not in SDK headers for these info classes.
// Load dynamically to avoid linker issues with older SDK.
typedef NTSTATUS(WINAPI* PFN_NtSetSystemInfo)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG);

// Undocumented info class values for memory management
static constexpr SYSTEM_INFORMATION_CLASS SystemMemoryListInformation =
    static_cast<SYSTEM_INFORMATION_CLASS>(0x50); // 80

enum SYSTEM_MEMORY_LIST_COMMAND : DWORD {
    MemoryFlushModifiedList = 3,
    MemoryPurgeStandbyList  = 4,
};

static PFN_NtSetSystemInfo s_NtSetSystemInfo = nullptr;

static void LoadNtdll() {
    if (s_NtSetSystemInfo) return;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"); // already loaded
    if (ntdll)
        s_NtSetSystemInfo = reinterpret_cast<PFN_NtSetSystemInfo>(
            GetProcAddress(ntdll, "NtSetSystemInformation"));
}

static bool PurgeStandbyList() {
    LoadNtdll();
    if (!s_NtSetSystemInfo) return false;
    SYSTEM_MEMORY_LIST_COMMAND cmd = MemoryPurgeStandbyList;
    NTSTATUS st = s_NtSetSystemInfo(SystemMemoryListInformation,
                                    &cmd, sizeof(cmd));
    return NT_SUCCESS(st);
}

static bool FlushModifiedList() {
    LoadNtdll();
    if (!s_NtSetSystemInfo) return false;
    SYSTEM_MEMORY_LIST_COMMAND cmd = MemoryFlushModifiedList;
    NTSTATUS st = s_NtSetSystemInfo(SystemMemoryListInformation,
                                    &cmd, sizeof(cmd));
    return NT_SUCCESS(st);
}

static bool ClearFileSystemCache() {
    // Requires SeIncreaseQuotaPrivilege (usually available with admin token)
    return SetSystemFileCacheSize(
        static_cast<SIZE_T>(-1),
        static_cast<SIZE_T>(-1),
        0) != FALSE;
}

int Cleaner_TrimWorkingSets() {
    DWORD pids[4096]{};
    DWORD bytesReturned = 0;
    if (!EnumProcesses(pids, sizeof(pids), &bytesReturned))
        return 0;

    int count = 0;
    DWORD numPids = bytesReturned / sizeof(DWORD);

    for (DWORD i = 0; i < numPids; ++i) {
        if (pids[i] == 0) continue; // System Idle

        HANDLE hProc = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA,
            FALSE, pids[i]);
        if (!hProc) continue;

        if (EmptyWorkingSet(hProc))
            ++count;

        CloseHandle(hProc);
    }
    return count;
}

int Cleaner_Run() {
    int trimmed = Cleaner_TrimWorkingSets();

    // These two require SeProfileSingleProcessPrivilege (admin)
    FlushModifiedList();
    PurgeStandbyList();
    ClearFileSystemCache();

    return trimmed;
}
