"""Cross-platform launcher that sets up PYTHONPATH/PATH and runs the demo.

Usage:
    uv run python samples/pyside_embed_demo/run_demo.py [options]

The script searches the xmake build tree for the rbc_editor_py.pyd module,
adds its directory to PYTHONPATH, and ensures dependent DLLs are on PATH.
"""

from __future__ import annotations

import os
import sys
import subprocess
from pathlib import Path


def _repo_root() -> Path:
    # This script lives at samples/pyside_embed_demo/run_demo.py.
    return Path(__file__).resolve().parents[2]


def _find_extension() -> Path | None:
    repo = _repo_root()
    search_roots = [
        repo / "build",
    ]
    # Allow the user to point directly at the build directory.
    env_build = os.environ.get("RBC_BUILD_DIR")
    if env_build:
        search_roots.insert(0, Path(env_build))

    ext_names = ["rbc_editor_py.pyd", "rbc_editor_py.so", "rbc_editor_py.dylib"]
    for root in search_roots:
        if not root.exists():
            continue
        for name in ext_names:
            for path in root.rglob(name):
                return path
    return None


def _xmake_build_dir() -> Path | None:
    try:
        result = subprocess.run(
            ["xmake", "show", "--buildir"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0 and result.stdout:
            path = Path(result.stdout.strip().splitlines()[0])
            if path.is_absolute():
                return path
            return _repo_root() / path
    except FileNotFoundError:
        pass
    return None


def _prepend_env(name: str, path: Path) -> None:
    existing = os.environ.get(name, "")
    if existing:
        os.environ[name] = f"{path}{os.pathsep}{existing}"
    else:
        os.environ[name] = str(path)


def main() -> int:
    ext_path = _find_extension()
    if ext_path is None:
        build_dir = _xmake_build_dir()
        print(
            "Error: could not find rbc_editor_py.pyd.\n"
            "Please build the project first:\n"
            "  xmake f -m releasedbg -c\n"
            "  xmake rbc_editor_py",
            file=sys.stderr,
        )
        if build_dir:
            print(f"Expected build directory: {build_dir}", file=sys.stderr)
        return 1

    ext_dir = ext_path.parent
    print(f"Found extension: {ext_path}")

    _prepend_env("PYTHONPATH", ext_dir)
    _prepend_env("PYTHONPATH", _repo_root())

    # Qt and other native dependencies are still sometimes resolved through
    # PATH on Windows, so make sure the extension directory and Qt bin are
    # available to the child process.
    _prepend_env("PATH", ext_dir)
    qt_bin = Path(r"D:\tools\Qt\6.9.3\msvc2022_64\bin")
    if qt_bin.exists():
        _prepend_env("PATH", qt_bin)

    # Re-execute with the updated environment so the Python import machinery
    # sees the new PYTHONPATH.
    if os.environ.get("_RBC_PYSIDE_DEMO_RESTARTED") != "1":
        os.environ["_RBC_PYSIDE_DEMO_RESTARTED"] = "1"
        args = [sys.executable, "-m", "samples.pyside_embed_demo.demo_app", *sys.argv[1:]]
        print(f"Launching: {' '.join(args)}")
        return subprocess.call(args, cwd=str(_repo_root()))

    # Should not reach here because the restarted process runs the module.
    return 1


if __name__ == "__main__":
    sys.exit(main())
