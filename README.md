<p align="center">
  <img src="res/logo.png" alt="RAMKeeper" width="400"/>
</p>

<p align="center">
  <strong>Lightweight Windows RAM Cleaner</strong><br/>
  149 KB · No runtime dependencies · No ads · Open source
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-blue?logo=windows" alt="Windows"/>
  <img src="https://img.shields.io/badge/size-149_KB-brightgreen" alt="Size"/>
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B17-orange?logo=cplusplus" alt="C++17"/>
</p>

---

# RAMKeeper

Lightweight Windows RAM cleaner — **149 KB**, no runtime dependencies, no ads.

Purges the Windows standby/cached memory list that Task Manager can't touch. Runs silently in the system tray.

---

## Features

- Tray-only (no taskbar button, no bloat)
- **Clean Now** — hotkey `Ctrl+Alt+R` or double-click tray icon
- Auto-clean when RAM exceeds threshold (default 80%)
- Interval clean, idle-triggered clean, boot-delay clean
- Balloon notifications (optional, silenceable)
- Single `.exe` — portable, no installer required
- Open source, MIT licensed

---

## Download

**[Latest Release →](../../releases/latest)**

Download `RAMKeeper.exe` from the Assets section and run as Administrator.

---

## Installation

### Option 1: Portable (recommended)

1. Download `RAMKeeper.exe` from [Releases](../../releases/latest)
2. Right-click → **Run as administrator** (required for standby list purge)
3. Tray icon appears — right-click for menu

### Option 2: Start with Windows (no UAC prompt)

Run once as admin, then right-click tray → **Settings** → enable **Start with Windows**.
This adds RAMKeeper to your registry `Run` key so it auto-starts.

For zero UAC prompts, create a Scheduled Task instead:

```
Task Scheduler → Create Task
  General: Run with highest privileges, configure for Windows 10
  Trigger: At log on
  Action: Start program → path\to\RAMKeeper.exe
```

---

## Usage

| Action | How |
|---|---|
| Clean RAM now | `Ctrl+Alt+R` or double-click tray icon |
| Toggle auto-clean | Right-click tray → Auto-clean |
| Exit | Right-click tray → Exit |

### Configuration

Config file at `%APPDATA%\RAMKeeper\config.ini` (created on first run):

```ini
[Clean]
threshold_percent = 80    ; clean when RAM usage exceeds this %
interval_minutes  = 30    ; force clean every N minutes (0 = disabled)
on_idle_minutes   = 5     ; clean when user idle N+ minutes (0 = disabled)
on_boot_delay     = 60    ; clean once N seconds after launch (0 = disabled)
silent_mode       = 0     ; 1 = suppress balloon notifications
auto_clean        = 1     ; 0 = disable all automatic cleaning

[App]
start_with_windows = 0    ; 1 = add to registry Run key
```

---

## Build from Source

### Prerequisites

| Tool | Install |
|---|---|
| Visual Studio 2022 Build Tools | `winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet"` |
| CMake 3.20+ | `winget install Kitware.CMake` |
| Ninja | `winget install Ninja-build.Ninja` |

See [setup-cpp.md](setup-cpp.md) for full setup guide including MinGW alternative.

### Build

Open **Developer Command Prompt for VS 2022**:

```bat
git clone https://github.com/ashishpatel26/RAMKeeper.git
cd RAMKeeper
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `build\RAMKeeper.exe`

### Verify no runtime dependencies

```bat
dumpbin /dependents build\RAMKeeper.exe
```

Expected: only `KERNEL32.dll`, `USER32.dll`, `SHELL32.dll`, `ADVAPI32.dll`.

---

## How It Works

Windows keeps unused pages in a "standby list" as a disk cache. This is useful but means RAM shows 80-95% used even when idle. RAMKeeper calls:

1. **`EmptyWorkingSet`** on every process — trims private working sets
2. **`NtSetSystemInformation(MemoryFlushModifiedList)`** — flushes modified pages to disk
3. **`NtSetSystemInformation(MemoryPurgeStandbyList)`** — clears the standby cache
4. **`SetSystemFileCacheSize(-1,-1,0)`** — resets file system cache size

Steps 2-4 require administrator privileges.

---

## Why Not Use Existing Tools?

| | RAMKeeper | Wise Memory Optimizer | Mem Reduct |
|---|---|---|---|
| Size | **149 KB** | 50 MB | 1.5 MB |
| No ads | ✅ | ❌ | ✅ |
| Open source | ✅ | ❌ | ❌ |
| Tray-only UI | ✅ | ❌ | ⚠ |
| Hotkey | ✅ | ❌ | ⚠ |

---

## License

MIT — see [LICENSE](LICENSE)
