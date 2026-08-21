"""Non-blocking validation of rbc_editor_py without PySide6.

This script can be run on CI/build machines to check that the C++ extension
module imports, exposes the expected API, and rejects create_viewport when no
QApplication exists.  It intentionally never creates a GUI window.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

# Allow running from the source tree without installing the package.
repo_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo_root / "src"))


def _add_dll_search_paths() -> None:
    """Add common Windows DLL search directories.

    Python 3.8+ does not use PATH for extension-module dependencies, so we
    register the build output directory and Qt runtime bin directory explicitly.
    """
    candidates = [
        repo_root / "build" / "windows" / "x64" / "release",
        repo_root / "build" / "windows" / "x64" / "releasedbg",
        Path(r"D:\tools\Qt\6.9.3\msvc2022_64\bin"),
        Path(r"C:\Users\Color\AppData\Roaming\uv\python\cpython-3.14.2-windows-x86_64-none"),
    ]
    for path in candidates:
        if path.exists() and hasattr(os, "add_dll_directory"):
            try:
                os.add_dll_directory(str(path))
                print(f"  add_dll_directory: {path}")
            except OSError as exc:
                print(f"  failed to add {path}: {exc}")


_add_dll_search_paths()

import rbc_editor_py


def main() -> int:
    errors = []

    print("Checking rbc_editor_py module...")
    print(f"  module file: {getattr(rbc_editor_py, '__file__', 'unknown')}")

    expected_functions = [
        "has_qapplication",
        "create_viewport",
        "destroy_viewport",
        "destroy_all_viewports",
        "viewport_info",
        "set_camera_distance",
        "set_clear_color",
        "reset_camera",
    ]
    for name in expected_functions:
        if not hasattr(rbc_editor_py, name):
            errors.append(f"Missing function: {name}")
        else:
            print(f"  + {name}")

    print("Checking has_qapplication()...")
    try:
        has_app = rbc_editor_py.has_qapplication()
        print(f"  has_qapplication() -> {has_app}")
    except Exception as exc:
        errors.append(f"has_qapplication() raised {exc}")

    print("Checking create_viewport without QApplication raises...")
    try:
        rbc_editor_py.create_viewport(
            program_path=str(repo_root / "src" / "robocute" / "rbc_ext" / "_C"),
            backend="dx",
            parent_wid=0,
        )
        errors.append("create_viewport() should have raised without QApplication")
    except RuntimeError as exc:
        print(f"  create_viewport correctly raised RuntimeError: {exc}")
    except Exception as exc:
        errors.append(f"create_viewport raised unexpected {type(exc).__name__}: {exc}")

    if errors:
        print(f"\nValidation failed with {len(errors)} error(s):")
        for err in errors:
            print(f"  - {err}")
        return 1

    print("\nValidation passed. GUI embedding must be tested interactively on a machine with PySide6.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
