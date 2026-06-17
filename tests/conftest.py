import ctypes
import pytest


def _is_elevated():
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "requires_admin: test requires Administrator privileges"
    )


def pytest_collection_modifyitems(items):
    if not _is_elevated():
        skip = pytest.mark.skip(reason="Run pytest as Administrator (RAMKeeper manifest requires elevation)")
        for item in items:
            item.add_marker(skip)
