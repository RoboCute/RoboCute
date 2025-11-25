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

# Import C++ extension types (if available)
try:
    import rbc_ext_c  # type: ignore
    
    # Re-export C++ types
    if hasattr(rbc_ext_c, 'AsyncResourceLoader'):
        AsyncResourceLoader = rbc_ext_c.AsyncResourceLoader
    if hasattr(rbc_ext_c, 'ResourceType'):
        ResourceType = rbc_ext_c.ResourceType
    if hasattr(rbc_ext_c, 'ResourceState'):
        ResourceState = rbc_ext_c.ResourceState
    if hasattr(rbc_ext_c, 'LoadPriority'):
        LoadPriority = rbc_ext_c.LoadPriority
        
except ImportError as e:
    print(f"Warning: Failed to import rbc_ext_c module: {e}")
    # Define placeholder if not available
    class AsyncResourceLoader:
        def __init__(self):
            raise NotImplementedError("C++ extension not available")