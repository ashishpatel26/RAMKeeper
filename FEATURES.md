# RAMKeeper — Project Feature Documentation

## Project Overview

| Attribute | Detail |
|-----------|--------|
| **App name** | RAMKeeper |
| **Language** | C++ (C++17), Win32 API only |
| **Build** | CMake + MSVC, static CRT, no external dependencies |
| **Binary size** | ~163 KB (fully self-contained) |
| **Architecture** | Message-only HWND (`HWND_MESSAGE`), single-instance mutex guard |
| **Purpose** | Windows system-tray utility that frees RAM on demand and automatically, using four OS-level memory reclaim strategies |
| **Distribution** | GitHub Releases (EXE + ZIP), winget (`ashishpatel26.RAMKeeper`), Chocolatey |
| **CI/CD** | GitHub Actions — builds, signs (SignPath Foundation — in progress), publishes release assets |

**Module layout**

```
src/main.cpp      — entry point, timer loop, hotkey, tray event dispatch
src/cleaner.cpp   — four RAM-reclaim strategies + process exclusion
src/config.cpp    — INI persistence + registry autostart
src/settings.cpp  — Win32 settings dialog (no resource file — built programmatically)
src/stats.cpp     — GlobalMemoryStatusEx wrapper
src/tray.cpp      — Shell_NotifyIconW wrapper, color icon, context menu
src/history.cpp   — RAM history ring buffer + clean event log
src/status.cpp    — dark mode status window with GDI sparkline
include/          — matching header for each module
```

---

## Current Features (by Version)

### v1.0.0 — Initial Release

> Source: tag `v1.0.0` → commit `9484a11` "Add GitHub Pages landing page" (includes all commits from `d91f6de` initial commit)

- **System tray icon** — registers with `Shell_NotifyIconW`; tooltip shows `"RAMKeeper\n8.3 / 16.0 GB (52%)"`, updated every second via `WM_TIMER`
- **Manual clean — tray menu** — right-click → "Clean Now" (bold default item)
- **Manual clean — double-click** — `WM_LBUTTONDBLCLK` on tray icon triggers `DoClean()`
- **Global hotkey** — `Ctrl+Alt+R` registered with `RegisterHotKey`
- **Four-stage RAM reclaim** (`Cleaner_Run`):
  1. `EmptyWorkingSet` on every running process (user-mode trim, no admin required)
  2. `NtSetSystemInformation(MemoryFlushModifiedList)` — flushes modified page list (requires `SeProfileSingleProcessPrivilege`)
  3. `NtSetSystemInformation(MemoryPurgeStandbyList)` — purges standby list (requires same)
  4. `SetSystemFileCacheSize(-1, -1, 0)` — clears file-system cache (requires `SeIncreaseQuotaPrivilege`)
- **Auto-clean engine** — four independent triggers checked every second:
  - **Threshold**: clean when RAM usage `>= thresholdPercent` (default 80 %, 5-min minimum re-clean gap)
  - **Interval**: clean every `intervalMinutes` minutes (default 30, 0 = disabled)
  - **Idle**: clean when user idle `>= onIdleMinutes` minutes (default 5, via `GetLastInputInfo`)
  - **Boot-delay**: single clean `onBootDelaySec` seconds after first launch (default 60)
- **Auto-clean toggle** — tray menu checkbox; state persisted to `config.ini` immediately
- **Balloon notification** — shows "Freed X MB (N processes trimmed)" after each clean; suppressed if `< 10 MB` freed or silent mode on
- **Config persistence** — INI file at `%APPDATA%\RAMKeeper\config.ini`; created with defaults on first run
- **Autostart** — writes/removes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\RAMKeeper`
- **Single-instance guard** — named mutex `RAMKeeper`; second launch exits immediately
- **Tray icon resilience** — listens for `WM_TaskbarCreated` (broadcast when Explorer restarts) and re-registers the icon
- **RAM stats** — `RamStats` struct: totalBytes, availBytes, usedBytes, usedPercent, usedGB, totalGB via `GlobalMemoryStatusEx`
- **GitHub Pages site** — dark-theme landing page at `ashishpatel26.github.io/RAMKeeper`

**Default config values**

| Key | Default | Meaning |
|-----|---------|---------|
| `threshold_percent` | 80 | Clean when RAM ≥ 80 % used |
| `interval_minutes` | 30 | Force clean every 30 min |
| `on_idle_minutes` | 5 | Clean after 5 min idle |
| `on_boot_delay` | 60 | One-shot clean 60 s after start |
| `silent_mode` | false | Show balloon notifications |
| `auto_clean` | true | Auto-clean enabled |
| `start_with_windows` | false | Do not autostart by default |

---

### v1.0.1 — Settings Dialog + CI/CD + Package Manifests

> Source: tag `v1.0.1` → commit `b33f43d` "Implement Settings dialog"
> Also includes: `8a34624`/`d18552b` (GitHub Actions CI/CD), `ca02730` (winget + Chocolatey manifests)

- **Settings dialog** — Win32 modal window built programmatically (no `.rc` resource file):
  - Numeric edit fields: RAM threshold %, interval minutes, idle timeout minutes, boot delay seconds
  - Checkboxes: Auto-clean enable, Silent mode, Start with Windows
  - OK button: saves all fields to `config.ini` + updates autostart registry key
  - Cancel / ESC: discards all changes
  - Centered on screen; tab-order correct; system UI font applied to all controls
- **Settings → `DlgState` pattern** — dialog receives an `AppConfig*` pointer via `lpCreateParams`; mutations are committed only on OK (`accepted = true`)
- **GitHub Actions CI/CD** — auto-build and release workflow; builds on push + publishes release assets
- **winget manifest** — `ashishpatel26.RAMKeeper` submitted for v1.0.0 and v1.0.1
- **Chocolatey package** — nuspec + install script for v1.0.1

---

### v1.0.2 — Distribution & Signing

> Source: commits `2e7f2c4`, `0fd0b18`, `82356f3`

- **ZIP release artifact** — CI now publishes both `RAMKeeper.exe` and `RAMKeeper.zip`
- **SmartScreen guidance** — README and website updated with unblock instructions
- **SignPath Foundation code signing** — attribution added; signing pipeline in progress
- **winget manifest v1.0.2** — `ashishpatel26.RAMKeeper` winget package updated
- **Chocolatey package** — nuspec + install script bumped to 1.0.2
- **GitHub Pages site** — updated with install tabs (winget/choco/manual), comparison table, config reference

---

### v1.0.3 — Settings Save Bug Fix

> Source: commits `74b11b4`, `a939b7a`

- **Fix: settings changes now persist** — root cause: `PostQuitMessage` was called inside the Settings dialog's `WM_DESTROY` path, which posted `WM_QUIT` to the main message loop and caused the entire app to exit before `Config_Save` completed. Replaced with `PostThreadMessage(WM_NULL)` to wake the nested `GetMessageW` loop without terminating the outer one. Silent regression — OK appeared to work but all changes were lost on next launch.
- **winget manifest v1.0.3** — package updated

---

### v1.1.0 — Status Window, History & Config Expansion

> Source: commit `d006bc3` "feat: v1.1.0 — status window, history, color icon, exclusions, scheduled clean"

- **Status window** — dark-mode popup (460×340) positioned above system tray:
  - Large color-coded RAM % (green/yellow/red)
  - GB usage subtitle
  - 5-minute GDI sparkline graph (area fill + line, double-buffered)
  - Recent cleans list (last 5 events: time, MB freed, process count)
  - Admin/limited mode badge (top-right)
  - Win11 dark title bar (`DWMWA_USE_IMMERSIVE_DARK_MODE`) + rounded corners (`DWMWA_WINDOW_CORNER_PREFERENCE`)
  - Closes on lose-focus or ESC
- **Status window access** — left-click tray icon toggles; tray right-click → "Status Window…"
- **RAM history ring buffer** — 300 samples (5 min at 1/sec) in `history.cpp`; feeds sparkline
- **Per-clean log** — appends UTF-8 lines to `%APPDATA%\RAMKeeper\clean.log` on every clean
- **Clean event log** — last 50 cleans in memory (`CleanEvent` ring buffer); shown in status window
- **Color-coded tray icon** — dynamically drawn 16×16 GDI icon: vertical bar fill, green < 60 % / yellow 60–80 % / red > 80 %; updated every second; previous icon destroyed on update
- **UAC detection** — checks `TokenElevation` at startup; first clean while non-elevated shows one-time balloon: "Not running as admin — standby & cache cleanup skipped"
- **Process exclusion list** — comma-separated exe names in Settings (e.g. `game.exe, vm.exe`); matching processes skipped by `EmptyWorkingSet` in `Cleaner_TrimWorkingSets`
- **Scheduled clean** — `cleanHour`/`cleanMinute` config fields (0–23 / 0–59, −1 = disabled); fires once per calendar day at the configured time via `GetLocalTime` check in `OnTimer`
- **Configurable notify threshold** — `notifyMinMB` config field (default 10); replaces hardcoded 10 MB gate in `DoClean`
- **Show status after clean** — `showStatusOnClean` config bool; opens status window automatically after each clean
- **Hotkey flash** — `Ctrl+Alt+R` in silent mode now shows a brief balloon confirming the clean fired
- **Settings dialog expanded** — new "Schedule & Notifications" section (hour, minute, notify MB, show-status checkbox) + "Process Exclusion" section (full-width text edit)
- **Binary size** — 163 KB (+14 KB from v1.0.3); no new external dependencies

**New config fields (v1.1.0)**

| Key | Default | Meaning |
|-----|---------|---------|
| `exclude_list` | *(empty)* | Comma-sep exe names to skip from WS trim |
| `clean_hour` | −1 | Scheduled clean hour (0–23), −1 = off |
| `clean_minute` | 0 | Scheduled clean minute (0–59) |
| `notify_min_mb` | 10 | Min freed MB to show balloon |
| `show_status_on_clean` | false | Open status window after each clean |

---

## Feature Status Matrix

| Feature | Introduced | Status | Notes |
|---------|-----------|--------|-------|
| System tray icon + tooltip | v1.0.0 | ✅ Active | Updates every 1 s |
| Manual clean (menu) | v1.0.0 | ✅ Active | Bold default menu item |
| Manual clean (double-click) | v1.0.0 | ✅ Active | `WM_LBUTTONDBLCLK` |
| Global hotkey Ctrl+Alt+R | v1.0.0 | ✅ Active | Flash in silent mode (v1.1.0) |
| Working-set trim | v1.0.0 | ✅ Active | Exclusion list added in v1.1.0 |
| Modified-list flush | v1.0.0 | ✅ Active | Admin privilege needed |
| Standby-list purge | v1.0.0 | ✅ Active | Admin privilege needed |
| File-system cache clear | v1.0.0 | ✅ Active | Admin privilege needed |
| Threshold-based auto-clean | v1.0.0 | ✅ Active | 5-min re-clean guard |
| Interval-based auto-clean | v1.0.0 | ✅ Active | Default 30 min |
| Idle-based auto-clean | v1.0.0 | ✅ Active | `GetLastInputInfo` |
| Boot-delay auto-clean | v1.0.0 | ✅ Active | One-shot per launch |
| Balloon notification | v1.0.0 | ✅ Active | Threshold configurable (v1.1.0) |
| Auto-clean toggle (tray) | v1.0.0 | ✅ Active | Persisted on change |
| INI config persistence | v1.0.0 | ✅ Active | `%APPDATA%\RAMKeeper\` |
| Autostart registry | v1.0.0 | ✅ Active | `HKCU\...\Run` |
| Single-instance mutex | v1.0.0 | ✅ Active | |
| Tray icon resilience | v1.0.0 | ✅ Active | WM_TaskbarCreated |
| GitHub Pages site | v1.0.0 | ✅ Active | |
| GitHub Actions CI/CD | v1.0.1 | ✅ Active | Auto-build + release |
| winget package | v1.0.1 | ✅ Active | v1.1.0 current |
| Chocolatey package | v1.0.1 | ✅ Active | v1.1.0 current |
| Settings dialog | v1.0.1 | ✅ Active | Bug fixed in v1.0.3, expanded in v1.1.0 |
| ZIP release artifact | v1.0.2 | ✅ Active | CI artifact |
| Code signing (SignPath) | v1.0.2 | 🔄 In progress | Signing pipeline wired |
| GitHub Release v1.0.2 | v1.0.2 | ✅ Active | Released |
| GitHub Release v1.0.3 | v1.0.3 | ✅ Active | Released |
| Status window | v1.1.0 | ✅ Active | Dark mode, sparkline, clean log |
| RAM history ring buffer | v1.1.0 | ✅ Active | 300 samples / 5 min |
| Per-clean log (file) | v1.1.0 | ✅ Active | `%APPDATA%\RAMKeeper\clean.log` |
| Color-coded tray icon | v1.1.0 | ✅ Active | Green/yellow/red, drawn with GDI |
| UAC detection + warning | v1.1.0 | ✅ Active | One-time balloon if not elevated |
| Process exclusion list | v1.1.0 | ✅ Active | Settings + `Cleaner_TrimWorkingSets` |
| Scheduled clean | v1.1.0 | ✅ Active | Daily at configured hour:minute |
| Configurable notify MB | v1.1.0 | ✅ Active | Default 10 MB |
| Show status after clean | v1.1.0 | ✅ Active | Config toggle |
| Win11 dark title bar | v1.1.0 | ✅ Active | DWM attribute 20 + 33 |

---

## Future Roadmap

> Items marked ~~strikethrough~~ shipped in v1.1.0.

### 🔜 Short-term (next release)

- **Signed binary** — SignPath pipeline wired but not completing; removing SmartScreen friction is the highest user-facing blocker
- **UAC re-launch with `runas`** — current approach shows one-time balloon; full fix is `ShellExecute` re-launch with `runas` verb to get an elevated token. Non-trivial: single-instance mutex, tray icon, and config state all need handling across the process restart
- **64-bit + ARM64 builds** — CI change only; ARM64 EC covers Surface Pro X and Copilot+ PCs

### 🗓️ Mid-term (3–6 months)

- **Memory history export** — ring buffer exists; add CSV/JSON export from status window or a copy-to-clipboard button
- **Scheduled clean UI polish** — current Settings fields are two separate int edits; a time picker (HH:MM) would be more intuitive
- **Tray tooltip: last-clean time** — show when the last clean ran in the tooltip alongside GB usage
- **Per-clean log viewer** — status window shows last 5 in-memory; a "View log…" button opening the `.log` file in Notepad would close the loop

### 🚀 Long-term (6+ months)

- **Multiple clean profiles** — single `AppConfig` struct; profile switching ("Gaming" vs "Work") requires profile selector in Settings + named INI sections
- **Notification customization** — custom sound, toast-style notification, suppression window
- **Localization** — all strings are hardcoded `L"..."` literals; extract to `.rc STRINGTABLE` for translations
- **Installer (MSI/NSIS)** — bare EXE/ZIP distribution; proper installer handles elevation, Start Menu shortcut, uninstall entry

---

## Technical Debt & Known Gaps

| Area | Gap | Severity |
|------|-----|---------|
| **UAC re-launch** | Admin ops fail silently below elevated token; v1.1.0 surfaces balloon but does not re-launch as admin | Medium |
| **`pids[4096]` cap** | `Cleaner_TrimWorkingSets` uses fixed 4096-entry PID array; systems with > 4096 processes miss some | Low (rare) |
| **`GetLastInputInfo` 32-bit rollover** | Tick-count domain mismatch acknowledged in source; benign but could miss one idle trigger every ~49 days | Low |
| **No settings cross-validation** | `GetEditInt` clamps ranges but no cross-field checks (e.g. scheduled minute with disabled hour) | Low |
| **No uninstall cleanup** | Autostart registry key and `%APPDATA%\RAMKeeper\` not removed on uninstall (no installer yet) | Low |
| **ntdll pointer never freed** | `LoadNtdll` caches `NtSetSystemInformation` for process lifetime — correct but undocumented assumption | Informational |
| **Status window WS_EX_NOACTIVATE dance** | `WS_EX_NOACTIVATE` set then cleared at creation to avoid stealing focus while still handling `WM_ACTIVATE` dismiss | Informational |
