# RAMKeeper — Project Feature Documentation

## Project Overview

| Attribute | Detail |
|-----------|--------|
| **App name** | RAMKeeper |
| **Language** | C++ (C++17), Win32 API only |
| **Build** | CMake + MSVC, static CRT, no external dependencies |
| **Binary size** | ~149 KB (fully self-contained) |
| **Architecture** | Message-only HWND (`HWND_MESSAGE`), single-instance mutex guard |
| **Purpose** | Windows system-tray utility that frees RAM on demand and automatically, using four OS-level memory reclaim strategies |
| **Distribution** | GitHub Releases (EXE + ZIP), winget (`ashishpatel26.RAMKeeper`), Chocolatey |
| **CI/CD** | GitHub Actions — builds, signs (SignPath Foundation), publishes release assets |

**Module layout**

```
src/main.cpp      — entry point, timer loop, hotkey, tray event dispatch
src/cleaner.cpp   — four RAM-reclaim strategies
src/config.cpp    — INI persistence + registry autostart
src/settings.cpp  — Win32 settings dialog (no resource file — built programmatically)
src/stats.cpp     — GlobalMemoryStatusEx wrapper
src/tray.cpp      — Shell_NotifyIconW wrapper + context menu
include/          — matching header for each module
```

---

## Current Features (by Version)

### v1.0.0 — Initial Release

> Source: commit `d91f6de` "Initial commit: RAMKeeper Win32 tray RAM cleaner"

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

### v1.0.1 — Settings Dialog

> Source: commit `b33f43d` "Implement Settings dialog (fix: clicking Settings now opens a window)"

- **Settings dialog** — Win32 modal window built programmatically (no `.rc` resource file):
  - Numeric edit fields: RAM threshold %, interval minutes, idle timeout minutes, boot delay seconds
  - Checkboxes: Auto-clean enable, Silent mode, Start with Windows
  - OK button: saves all fields to `config.ini` + updates autostart registry key
  - Cancel / ESC: discards all changes
  - Centered on screen; tab-order correct; system UI font applied to all controls
- **Settings → `DlgState` pattern** — dialog receives an `AppConfig*` pointer via `lpCreateParams`; mutations are committed only on OK (`accepted = true`)

---

### v1.0.2 — Distribution & Signing

> Source: commits `2e7f2c4`, `0fd0b18`, `82356f3`

- **ZIP release artifact** — CI now publishes both `RAMKeeper.exe` and `RAMKeeper.zip`
- **SmartScreen guidance** — README and website updated with unblock instructions
- **SignPath Foundation code signing** — attribution added; signing pipeline in progress
- **winget manifest v1.0.2** — `ashishpatel26.RAMKeeper` winget package updated
- **Chocolatey package** — nuspec + install script bumped to 1.0.2
- **GitHub Pages site** — dark-theme single-page site at `ashishpatel26.github.io/RAMKeeper` covering features, install tabs (winget/choco/manual), comparison table, config reference

---

### v1.0.3 — Settings Save Bug Fix

> Source: commits `74b11b4`, `a939b7a`

- **Fix: settings changes now persist** — root cause: `PostQuitMessage` was called inside the child dialog's destroy path, which terminated the message loop of the *parent* window before `Config_Save` completed; replaced with `PostThreadMessage(WM_NULL)` to avoid premature loop exit
- **winget manifest v1.0.3** — package updated

---

## Feature Status Matrix

| Feature | Introduced | Status | Notes |
|---------|-----------|--------|-------|
| System tray icon + tooltip | v1.0.0 | ✅ Active | Updates every 1 s |
| Manual clean (menu) | v1.0.0 | ✅ Active | Bold default menu item |
| Manual clean (double-click) | v1.0.0 | ✅ Active | `WM_LBUTTONDBLCLK` |
| Global hotkey Ctrl+Alt+R | v1.0.0 | ✅ Active | `RegisterHotKey` |
| Working-set trim | v1.0.0 | ✅ Active | No admin required |
| Modified-list flush | v1.0.0 | ✅ Active | Admin privilege needed |
| Standby-list purge | v1.0.0 | ✅ Active | Admin privilege needed |
| File-system cache clear | v1.0.0 | ✅ Active | Admin privilege needed |
| Threshold-based auto-clean | v1.0.0 | ✅ Active | 5-min re-clean guard |
| Interval-based auto-clean | v1.0.0 | ✅ Active | Default 30 min |
| Idle-based auto-clean | v1.0.0 | ✅ Active | `GetLastInputInfo` |
| Boot-delay auto-clean | v1.0.0 | ✅ Active | One-shot per launch |
| Balloon notification | v1.0.0 | ✅ Active | Suppressed < 10 MB freed |
| Auto-clean toggle (tray) | v1.0.0 | ✅ Active | Persisted on change |
| INI config persistence | v1.0.0 | ✅ Active | `%APPDATA%\RAMKeeper\` |
| Autostart registry | v1.0.0 | ✅ Active | `HKCU\...\Run` |
| Single-instance mutex | v1.0.0 | ✅ Active | |
| Tray icon resilience | v1.0.0 | ✅ Active | WM_TaskbarCreated |
| Settings dialog | v1.0.1 | ✅ Active | Bug fixed in v1.0.3 |
| ZIP release artifact | v1.0.2 | ✅ Active | CI artifact |
| Code signing (SignPath) | v1.0.2 | 🔄 In progress | Signing pipeline wired |
| winget package | v1.0.0 | ✅ Active | v1.0.3 current |
| Chocolatey package | v1.0.1 | ✅ Active | v1.0.3 current |
| GitHub Pages site | v1.0.0 | ✅ Active | |

---

## Future Roadmap

> No `TODO`/`FIXME` comments exist in source. All items below are **inferred** from architectural gaps and common practice for this class of tool.

### 🔜 Short-term (next release)

- **UAC elevation prompt** — admin-requiring operations (`FlushModifiedList`, `PurgeStandbyList`, `ClearFileSystemCache`) silently no-op when not elevated; surface a one-time prompt or ShellExecute re-launch with `runas` verb
- **Signed binary** — SignPath pipeline is wired but signing is noted as "in progress" in v1.0.2 commit; completing this removes SmartScreen friction for new users
- **Process count in tooltip** — `Cleaner_TrimWorkingSets` returns a count; expose it in the post-clean balloon but not yet in the tray tooltip or persistent log
- **Clean confirmation on hotkey** — `Ctrl+Alt+R` fires silently in silent mode; a brief tray tooltip flash would confirm the action fired

### 🗓️ Mid-term (3–6 months)

- **Process exclusion list** — no mechanism to skip specific processes (e.g. games, VMs) from `EmptyWorkingSet`; an exclusion list in Settings would prevent latency spikes on excluded apps
- **Memory history graph** — `RamStats` is sampled every second but never stored; a small ring buffer + simple tray-click graph window would add diagnostic value
- **Scheduled clean times** — current triggers are elapsed-time only; a "clean at 02:00 daily" cron-style trigger would suit server/workstation use
- **Tray icon color coding** — static icon; color/badge change at threshold crossings (green/yellow/red) would make state visible at a glance without hovering
- **Per-clean log** — no audit trail; a rolling `clean.log` in `%APPDATA%\RAMKeeper\` would help users verify the tool is working

### 🚀 Long-term (6+ months)

- **Multiple clean profiles** — single `AppConfig` struct; profile switching (e.g. "Gaming" vs "Work") would require a profile selector in Settings and named INI sections
- **Notification customization** — balloon threshold (currently hardcoded 10 MB) not user-configurable; custom sound, toast style, or suppression window would improve UX
- **Localization** — all strings are hardcoded `L"..."` literals in source; extracting to a string table (`.rc STRINGTABLE`) would enable translations
- **Installer (MSI/NSIS)** — currently distributed as bare EXE/ZIP; a proper installer would handle elevation, Start Menu shortcut, and uninstall entry
- **64-bit + ARM64 builds** — CI currently targets one architecture; adding ARM64 EC would cover Surface Pro X and Copilot+ PCs

---

## Technical Debt & Known Gaps

| Area | Gap | Severity |
|------|-----|---------|
| **Error visibility** | Admin-required ops fail silently when not elevated — user sees no feedback | Medium |
| **`pids[4096]` cap** | `Cleaner_TrimWorkingSets` uses a fixed 4096-entry PID array; systems with > 4096 processes would miss some | Low (rare) |
| **`GetLastInputInfo` 32-bit rollover** | Comment in source acknowledges tick-count domain mismatch; benign in practice but could cause a missed idle trigger every ~49 days | Low |
| **No settings validation** | `GetEditInt` clamps to `[0, 9999]` but no cross-field validation (e.g. interval < threshold gap) | Low |
| **Hardcoded 10 MB notify threshold** | `DoClean` suppresses balloon if freed < 10 MB; not user-configurable | Low |
| **No uninstall cleanup** | Autostart registry key and `%APPDATA%\RAMKeeper\` are not removed on uninstall (no installer exists yet) | Low |
| **ntdll function loaded once, never freed** | `LoadNtdll` caches `NtSetSystemInformation` pointer for process lifetime — correct but undocumented assumption | Informational |
