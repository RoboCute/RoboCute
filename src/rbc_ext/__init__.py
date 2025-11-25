import sys
import os
from pathlib import Path

# append installed ext path
sys.path.append(os.path.join(os.path.dirname(__file__), "release"))

# Auto-setup RBC_RUNTIME_DIR if not set
if "RBC_RUNTIME_DIR" not in os.environ:
    project_root = Path(__file__).parent.parent.parent
    # Try to find the build directory
    for build_mode in ["release", "debug"]:
        runtime_dir = project_root / "build" / "windows" / "x64" / build_mode
        if runtime_dir.exists() and (runtime_dir / "py_backend_impl.dll").exists():
            os.environ["RBC_RUNTIME_DIR"] = str(runtime_dir)
            # Also add to PATH for DLL loading
            os.environ["PATH"] = f"{runtime_dir};{os.environ.get('PATH', '')}"
            print(f"Auto-detected RBC_RUNTIME_DIR: {runtime_dir}")
            break
