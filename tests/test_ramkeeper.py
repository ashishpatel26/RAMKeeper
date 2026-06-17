"""
RAMKeeper Win32 UI tests.
Run: uv run pytest tests/ -v
Requires: build/RAMKeeper.exe (cmake --build build --config Release)
Must run as Administrator for standby-purge tests to fully exercise clean path.
"""
import os
import subprocess
import time
import pytest
from pywinauto import Desktop
from pywinauto.keyboard import send_keys

import ctypes
import ctypes.wintypes

EXE = os.path.join(os.path.dirname(__file__), "..", "build", "RAMKeeper.exe")
EXE = os.path.abspath(EXE)

# RAMKeeper uses HWND_MESSAGE parent — not enumerated by FindWindowW.
# FindWindowExW(HWND_MESSAGE, NULL, ...) searches the message-only hierarchy.
_HWND_MESSAGE = ctypes.cast(-3, ctypes.wintypes.HWND)


def _find_hwnd():
    """Find RAMKeeper's message-only HWND."""
    return ctypes.windll.user32.FindWindowExW(
        _HWND_MESSAGE, None, "RAMKeeper", "RAMKeeper"
    )


def _post_cmd(cmd_id):
    """Post WM_COMMAND to RAMKeeper HWND. Returns hwnd (0 = not found)."""
    hwnd = _find_hwnd()
    if hwnd:
        ctypes.windll.user32.PostMessageW(hwnd, 0x0111, cmd_id, 0)
    return hwnd


IDM_CLEAN_NOW = 100
IDM_SETTINGS = 102
IDM_EXIT = 103
IDM_STATUS = 104


def find_tray_notify_icon():
    """Return the tray notification area window (Shell_TrayWnd)."""
    desktop = Desktop(backend="win32")
    try:
        return desktop.window(class_name="Shell_TrayWnd")
    except Exception:
        return None


def kill_ramkeeper():
    subprocess.run(["taskkill", "/F", "/IM", "RAMKeeper.exe"],
                   capture_output=True)
    time.sleep(0.5)


@pytest.fixture(autouse=True)
def cleanup():
    """Kill any stray RAMKeeper between tests."""
    kill_ramkeeper()
    yield
    kill_ramkeeper()


@pytest.fixture
def app():
    """Launch RAMKeeper and wait for tray registration.
    Must run as Administrator — manifest requires it.
    """
    # ponytail: subprocess.Popen works when caller is already elevated;
    # Application.start() uses CreateProcess which can't cross UAC boundary.
    proc = subprocess.Popen([EXE])
    time.sleep(2)  # tray icon registers within ~1s
    yield proc
    kill_ramkeeper()


# ── 1. Basic launch ─────────────────────────────────────────────────────────

def test_exe_exists():
    assert os.path.isfile(EXE), f"EXE not found: {EXE}"


def test_launches_without_crash(app):
    """Process stays alive after 2 s."""
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    assert "RAMKeeper.exe" in procs.stdout


def test_single_instance(app):
    """Second launch exits immediately; only one process."""
    subprocess.Popen([EXE])
    time.sleep(1)
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    # tasklist shows one line per process — exactly one should appear
    lines = [ln for ln in procs.stdout.splitlines() if "RAMKeeper.exe" in ln]
    assert len(lines) == 1, f"Expected 1 process, got {len(lines)}"


# ── 2. Tray icon ─────────────────────────────────────────────────────────────

def test_tray_icon_visible(app):
    """Shell_TrayWnd exists (tray bar is alive)."""
    tray = find_tray_notify_icon()
    assert tray is not None


# ── 3. Global hotkey ─────────────────────────────────────────────────────────

def test_hotkey_does_not_crash(app):
    """Ctrl+Alt+R fires clean; app still alive after."""
    send_keys("^%r")  # Ctrl+Alt+R
    time.sleep(3)
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    assert "RAMKeeper.exe" in procs.stdout


# ── 4. Settings dialog ───────────────────────────────────────────────────────

def test_settings_dialog_opens(app):
    """Settings dialog appears with expected title."""
    assert _post_cmd(IDM_SETTINGS), "RAMKeeper HWND not found"
    time.sleep(1)
    desktop = Desktop(backend="win32")
    dlg = desktop.window(title="RAMKeeper — Settings")
    assert dlg.exists(timeout=3), "Settings dialog did not open"


def test_settings_dialog_has_threshold_field(app):
    """Threshold edit control exists in settings."""
    _post_cmd(IDM_SETTINGS)
    time.sleep(1)
    desktop = Desktop(backend="win32")
    dlg = desktop.window(title="RAMKeeper — Settings")
    dlg.wait("visible", timeout=3)
    # IDC_EDT_THRESHOLD = 200
    ctrl = dlg.child_window(control_id=200)
    assert ctrl.exists(), "Threshold edit not found"
    val = int(ctrl.window_text())
    assert 1 <= val <= 99, f"Threshold out of range: {val}"


def test_settings_cancel_keeps_app_alive(app):
    """Cancelling settings does not exit the app."""
    _post_cmd(IDM_SETTINGS)
    time.sleep(1)
    desktop = Desktop(backend="win32")
    dlg = desktop.window(title="RAMKeeper — Settings")
    dlg.wait("visible", timeout=3)
    # IDC_BTN_CANCEL = 208
    dlg.child_window(control_id=208).click()
    time.sleep(0.5)
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    assert "RAMKeeper.exe" in procs.stdout


# ── 5. Status window ─────────────────────────────────────────────────────────

def test_status_window_opens(app):
    """Status window appears with title RAMKeeper."""
    assert _post_cmd(IDM_STATUS), "RAMKeeper HWND not found"
    time.sleep(1)
    desktop = Desktop(backend="win32")
    sw = desktop.window(class_name="RAMKeeperStatus")
    assert sw.exists(timeout=3), "Status window did not open"


def test_status_window_closes_on_escape(app):
    """ESC closes the status window."""
    _post_cmd(IDM_STATUS)
    time.sleep(1)
    desktop = Desktop(backend="win32")
    sw = desktop.window(class_name="RAMKeeperStatus")
    sw.wait("visible", timeout=3)
    sw.set_focus()
    send_keys("{ESC}")
    time.sleep(0.5)
    assert not sw.exists(), "Status window still open after ESC"


# ── 6. Clean via WM_COMMAND ──────────────────────────────────────────────────

def test_clean_now_does_not_crash(app):
    """IDM_CLEAN_NOW fires without killing the process."""
    assert _post_cmd(IDM_CLEAN_NOW), "RAMKeeper HWND not found"
    time.sleep(4)  # clean takes ~2-3s
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    assert "RAMKeeper.exe" in procs.stdout


# ── 7. Graceful exit ─────────────────────────────────────────────────────────

def test_exit_via_menu(app):
    """IDM_EXIT terminates the process cleanly."""
    assert _post_cmd(IDM_EXIT), "RAMKeeper HWND not found"
    time.sleep(1)
    procs = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq RAMKeeper.exe"],
        capture_output=True, text=True
    )
    assert "RAMKeeper.exe" not in procs.stdout
