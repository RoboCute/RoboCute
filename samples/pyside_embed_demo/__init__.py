"""Single-process PySide6 + LuisaCompute viewport embedding demo package.

This package is intentionally small and self-contained.  It demonstrates how an
external PySide6 front-end can host the native RoboCute LC viewport widget in
the same Python process.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

__version__ = "0.1.0-demo"
__all__ = ["LuisaViewportWidget", "MainWindow", "run_demo"]


def _add_dll_search_paths() -> None:
    """Register Windows DLL search directories used by the C++ extension.

    Python 3.8+ on Windows ignores PATH for extension-module dependencies, so
    we explicitly add the likely build output and Qt bin directories.  This is
    a no-op on non-Windows platforms and when the directories do not exist.
    """
    if not hasattr(os, "add_dll_directory"):
        return

    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "build" / "windows" / "x64" / "release",
        repo_root / "build" / "windows" / "x64" / "releasedbg",
        Path(r"D:\tools\Qt\6.9.3\msvc2022_64\bin"),
        Path(r"C:\Users\Color\AppData\Roaming\uv\python\cpython-3.14.2-windows-x86_64-none"),
    ]
    for path in candidates:
        if path.exists():
            try:
                os.add_dll_directory(str(path))
            except OSError:
                pass


_add_dll_search_paths()
