import json
import os
import platform as _platform
import shutil
from pathlib import Path

import requests

from rbc_build.utils import (
    compute_hash,
    get_project_root,
    print_debug,
    print_info,
    print_success,
    unzip_dir,
)

GIT_TASKS = {
    "lc": {
        "subdir": "thirdparty/LuisaCompute",
        "url": "https://github.com/LuisaGroup/LuisaCompute.git",
        "branch": "next",
        "deps": [],
    },
    "xxhash": {
        "subdir": "thirdparty/LuisaCompute/src/ext/xxhash",
        "url": "https://github.com/Cyan4973/xxHash.git",
        "branch": None,
        "deps": ["lc"],
    },
    "spdlog": {
        "subdir": "thirdparty/LuisaCompute/src/ext/spdlog",
        "url": "https://github.com/LuisaGroup/spdlog.git",
        "branch": None,
        "deps": ["lc"],
    },
    "EASTL": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL",
        "url": "https://github.com/LuisaGroup/EASTL.git",
        "branch": None,
        "deps": ["lc"],
    },
    "EABase": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL/packages/EABase",
        "url": "https://github.com/LuisaGroup/EABase.git",
        "branch": None,
        "deps": ["EASTL"],
    },
    "HIPRT": {
        "subdir": "thirdparty/LuisaCompute/src/ext/HIPRT",
        "url": "https://github.com/LuisaGroup/HIPRT.git",
        "branch": None,
        "deps": ["lc"],
    },
    "SPIRV-Tools": {
        "subdir": "thirdparty/LuisaCompute/src/ext/SPIRV-Tools",
        "url": "https://github.com/LuisaGroup/SPIRV-Tools.git",
        "branch": None,
        "deps": ["lc"],
    },
    "spirv-headers": {
        "subdir": "thirdparty/LuisaCompute/src/ext/spirv-headers",
        "url": "https://github.com/KhronosGroup/SPIRV-Headers.git",
        "branch": None,
        "deps": ["lc"],
    },
    "mimalloc": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL/packages/mimalloc",
        "url": "https://github.com/LuisaGroup/mimalloc.git",
        "branch": None,
        "deps": ["EASTL"],
    },
    "glfw": {
        "subdir": "thirdparty/LuisaCompute/src/ext/glfw",
        "url": "https://github.com/glfw/glfw.git",
        "branch": None,
        "deps": ["lc"],
    },
    "glslang": {
        "subdir": "thirdparty/LuisaCompute/src/ext/glslang",
        "url": "https://github.com/LuisaGroup/glslang",
        "branch": None,
        "deps": ["lc"],
    },
    "magic_enum": {
        "subdir": "thirdparty/LuisaCompute/src/ext/magic_enum",
        "url": "https://github.com/LuisaGroup/magic_enum",
        "branch": None,
        "deps": ["lc"],
    },
    "marl": {
        "subdir": "thirdparty/LuisaCompute/src/ext/marl",
        "url": "https://github.com/LuisaGroup/marl.git",
        "branch": None,
        "deps": ["lc"],
    },
    "reproc": {
        "subdir": "thirdparty/LuisaCompute/src/ext/reproc",
        "url": "https://github.com/LuisaGroup/reproc.git",
        "branch": None,
        "deps": ["lc"],
    },
    "stb": {
        "subdir": "thirdparty/LuisaCompute/src/ext/stb/stb",
        "url": "https://github.com/nothings/stb.git",
        "branch": None,
        "deps": ["lc"],
    },
    "yyjson": {
        "subdir": "thirdparty/LuisaCompute/src/ext/yyjson",
        "url": "https://github.com/ibireme/yyjson.git",
        "branch": None,
        "deps": ["lc"],
    },
    "pybind11": {
        "subdir": "thirdparty/LuisaCompute/src/ext/pybind11",
        "url": "https://github.com/LuisaGroup/pybind11.git",
        "branch": None,
        "deps": ["lc"],
    },
    "qt_node_editor": {
        "subdir": "thirdparty/qt_node_editor",
        "url": "https://github.com/RoboCute/nodeeditor.git",
        "branch": None,
        "deps": [],
    },
    "ozz_animation": {
        "subdir": "thirdparty/ozz_animation",
        "url": "https://github.com/RoboCute/ozz-animation.git",
        "branch": None,
        "deps": [],
    },
    # "alembic": {
    #     "subdir": "thirdparty/alembic",
    #     "url": "https://github.com/RoboCute/alembic.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "Imath": {
    #     "subdir": "thirdparty/Imath",
    #     "url": "https://github.com/AcademySoftwareFoundation/Imath.git",
    #     "branch": None,
    #     "deps": [],
    # },
    "lib3ds": {
        "subdir": "thirdparty/lib3ds",
        "url": "https://github.com/vkocheryzhkin/lib3ds.git",
        "branch": None,
        "deps": [],
    },
    "tinyply": {
        "subdir": "thirdparty/tinyply",
        "url": "https://github.com/ddiakopoulos/tinyply.git",
        "branch": None,
        "deps": [],
    },
    "tinyxml2": {
        "subdir": "thirdparty/tinyxml2",
        "url": "https://github.com/leethomason/tinyxml2.git",
        "branch": None,
        "deps": [],
    },
    # robotics
    # "urdfdom_headers": {
    #     "subdir": "thirdparty/urdfdom_headers",
    #     "url": "https://github.com/ros/urdfdom_headers.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "console_bridge": {
    #     "subdir": "thirdparty/console_bridge",
    #     "url": "https://github.com/ros/console_bridge.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "urdfdom": {
    #     "subdir": "thirdparty/urdfdom",
    #     "url": "https://github.com/ros/urdfdom.git",
    #     "branch": None,
    #     "deps": ["urdfdom_headers", "tinyxml2", "console_bridge"],
    # },
    # "jolt_physics": {
    #     "subdir": "thirdparty/jolt_physics",
    #     "url": "https://github.com/RoboCute/JoltPhysics.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "acl": {
    #     "subdir": "thirdparty/acl",
    #     "url": "https://github.com/RoboCute/acl.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "cpptrace": {
    #     "subdir": "thirdparty/cpptrace",
    #     "url": "https://github.com/RoboCute/cpptrace.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "libigl": {
    #     "subdir": "thirdparty/libigl",
    #     "url": "https://github.com/RoboCute/libigl.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "eigen": {
    #     "subdir": "thirdparty/eigen",
    #     "url": "https://github.com/RoboCute/eigen.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
    # "cppitertools": {
    #     "subdir": "thirdparty/cppitertools",
    #     "url": "https://github.com/RoboCute/cppitertools.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
    # "dylib": {
    #     "subdir": "thirdparty/dylib",
    #     "url": "https://github.com/RoboCute/dylib.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
}
OIDN_NAME = "oidn-2.3.3"
SHADER_PATH = "rbc/shader"
CLANGD_NAME = "clangd-v19.1.7"
LC_SDK_ADDRESS = "https://github.com/LuisaGroup/SDKs/releases/download/sdk/"
RBC_SDK_ADDRESS = (
    "https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/"
)
SKR_SDK_ADDRESS = (
    "https://github.com/SakuraEngine/Sakura.Resources/releases/download/SDKs/"
)
LC_DX_SDK = "dx_sdk_20250816.zip"
RENDER_RESOURCE_NAME = "render_resources-v1.0.1.7z"
XMAKE_GLOBAL_TOOLCHAIN = "clang-cl"

LLVM_VERSION = "21.1.1"
LLVM_SDK_NAME = f"llvm-{LLVM_VERSION}-release"
LLVM_INSTALL_DIR = f"build/download/llvm-{LLVM_VERSION}"

# Detect system platform and architecture
_system = _platform.system().lower()
_machine = _platform.machine().lower()

if _system == "windows":
    PLATFORM = "windows"
elif _system == "linux":
    PLATFORM = "linux"
elif _system == "darwin":
    PLATFORM = "macos"
else:
    PLATFORM = _system

# Normalize architecture names
if _machine in ("amd64", "x86_64", "x64"):
    if PLATFORM == "windows":
        ARCH = "x64"
    else:
        ARCH = "x86_x64"
elif _machine in ("aarch64", "arm64"):
    ARCH = "arm64"
elif _machine in ("i386", "i686", "x86"):
    ARCH = "x86"
else:
    ARCH = _machine


def _to_platform_spec(name):
    return f"{name}-{PLATFORM}-{ARCH}.7z"


if PLATFORM == "linux":
    LC_DX_SDK = "linux_dxc_2025_07_14.x86_64.zip"
elif PLATFORM != "windows":
    LC_DX_SDK = None

OIDN_NAME = _to_platform_spec(OIDN_NAME)
CLANGD_NAME = _to_platform_spec(CLANGD_NAME)
# PYD_TOOLCHAIN = "msvc"



def get_http_proxies():
    """Get HTTP/HTTPS proxy settings from environment variables."""
    proxies = {}
    http_proxy = os.environ.get("HTTP_PROXY") or os.environ.get("http_proxy")
    https_proxy = os.environ.get("HTTPS_PROXY") or os.environ.get("https_proxy")

    if http_proxy:
        proxies["http"] = http_proxy
        print_info(f"Using HTTP_PROXY: {http_proxy}")
    if https_proxy:
        proxies["https"] = https_proxy
        print_info(f"Using HTTPS_PROXY: {https_proxy}")

    return proxies if proxies else None


def get_requests_session():
    """Create a requests session with proxy settings from environment."""
    session = requests.Session()
    proxies = get_http_proxies()
    if proxies:
        session.proxies.update(proxies)
        print_info(f"Using proxy: {proxies}")
    return session


def fetch_sdk_manifest() -> dict[str, str]:
    """Download and parse the SDK manifest from the resource repository."""
    url = SKR_SDK_ADDRESS + "manifest.json"
    session = get_requests_session()
    response = session.get(url, timeout=30)
    response.raise_for_status()
    manifest = response.json()
    if not isinstance(manifest, dict):
        raise RuntimeError("SDK manifest.json is not an object")
    return {k: v.lower() for k, v in manifest.items()}


def install_sdk(
    name: str,
    mappings: dict[str, str],
    *,
    plat_postfix: bool = True,
) -> Path:
    """Download, verify, extract and install an SDK archive.

    The archive name follows ``{name}-{PLATFORM}-{ARCH}.zip`` when
    ``plat_postfix`` is true, otherwise ``{name}.zip``.  It is downloaded
    from ``SKR_SDK_ADDRESS`` and verified against the SHA256 recorded in
    ``manifest.json`` before being extracted to ``build/download/SDKs/<name>/``
    and copied to the final locations described by ``mappings``.
    """
    manifest = fetch_sdk_manifest()
    filename = f"{name}-{PLATFORM}-{ARCH}.zip" if plat_postfix else f"{name}.zip"
    if filename.lower() not in manifest:
        raise RuntimeError(f"SDK {filename} not found in manifest")

    expected_sha = manifest[filename.lower()]
    project_root = Path(get_project_root())
    download_file = project_root / "build/download" / filename
    sdk_extract_dir = project_root / "build/download/SDKs" / name

    # Download and verify SHA256.
    if not download_file.exists() or compute_hash(download_file) != expected_sha:
        print_info(f"Downloading SDK {filename} ...")
        session = get_requests_session()
        response = session.get(SKR_SDK_ADDRESS + filename, stream=True)
        response.raise_for_status()
        download_file.parent.mkdir(parents=True, exist_ok=True)
        with open(download_file, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
        actual_sha = compute_hash(download_file)
        if actual_sha != expected_sha:
            download_file.unlink(missing_ok=True)
            raise RuntimeError(
                f"SHA256 mismatch for {filename}: expected {expected_sha}, got {actual_sha}"
            )
        print_success(f"Downloaded SDK {filename}")
    else:
        print_debug(f"SDK {filename} already exists and SHA256 matches, skip download.")

    # Extract to the intermediate SDK directory.
    if sdk_extract_dir.exists():
        shutil.rmtree(sdk_extract_dir)
    sdk_extract_dir.mkdir(parents=True)
    unzip_dir(download_file, sdk_extract_dir)

    # Copy mapped entries to their final locations.
    for src_rel, dst_rel in mappings.items():
        src = sdk_extract_dir / src_rel
        dst = project_root / dst_rel
        if not src.exists():
            raise RuntimeError(f"SDK mapping source not found: {src}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.is_dir():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
        else:
            shutil.copy2(src, dst)

    return sdk_extract_dir
