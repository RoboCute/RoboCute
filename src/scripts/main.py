import os
import sys
import subprocess
import json
import shutil
import requests
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, wait
import time
import importlib
# from scripts.thirdparty_config import make_alembic_config, make_imath_config
from scripts.prepare import (
    GIT_TASKS,
    CLANGCXX_NAME,
    CLANGCXX_PATH,
    SHADER_PATH,
    CLANGD_NAME,
    LC_SDK_ADDRESS,
    RBC_SDK_ADDRESS,
    LC_DX_SDK,
    RENDER_RESOURCE_NAME,
    PLATFORM,
    ARCH,
    XMAKE_GLOBAL_TOOLCHAIN,
    OIDN_NAME,
)
from scripts.utils import is_empty_folder, get_project_root, rel, compute_hash, unzip_dir, print_success, print_error, print_warning, print_info, print_debug, run_git_command

PROJECT_ROOT = get_project_root()


def write_shader_compile_cmd():
    clangcxx_dir = rel(CLANGCXX_PATH)
    shader_dir = rel(SHADER_PATH)
    in_dir = shader_dir / "src"
    host_dir = shader_dir / "host"
    include_dir = shader_dir / "include"
    cache_dir = ""

    def base_cmd():
        return (
            f'"{clangcxx_dir}" -in="{in_dir}" -out="{out_dir}" -include="{include_dir}"'
        )

    def build_cmd():
        return base_cmd() + f' -hostgen="{host_dir}"' + f' -cache_dir="{cache_dir}"'

    backends = ["dx"]  # , "vk"
    # write files
    for backend in backends:
        cache_dir = shader_dir / ".cache" / backend
        out_dir = Path(PROJECT_ROOT) / \
            f"build/{PLATFORM}/{ARCH}/shader_build_{backend}"
        f = open(shader_dir / f"{backend}_compile.cmd", "w")
        f.write("@echo off\n" + build_cmd() + f" -backend={backend}")
        f.close()

        f = open(shader_dir / f"{backend}_clean_compile.cmd", "w")
        f.write("@echo off\n" + build_cmd() +
                f" -backend={backend}" + " -rebuild")
        f.close()

    out_dir = shader_dir / ".vscode/compile_commands.json"
    f = open(shader_dir / "gen_json.cmd", "w")
    f.write("@echo off\n" + base_cmd() + " -lsp")
    f.close()


def git_clone_or_pull(git_address, subdir, branch=None):
    abs_subdir = rel(subdir)
    args = []
    if is_empty_folder(abs_subdir):
        # Clone
        args = ["clone", git_address]
        if branch:
            args.extend(["-b", branch])
        args.append(abs_subdir)
        print_info(f"pulling {git_address} to {abs_subdir}")
    else:
        # Pull
        args = ["-C", abs_subdir, "pull"]
        if branch:
            args.extend(["origin", branch])
        print_info(f"pulling {git_address} to {abs_subdir}")

    done, log = run_git_command(args)
    log = log.strip()
    if done:
        print_success(log)
    else:
        print_error(log)
        sys.exit(1)


new_file_hash = {}
download_path = Path(PROJECT_ROOT) / "build/download"
hash_json_path = download_path / "file_hash.json"
tool_path = Path(PROJECT_ROOT) / "build/tool"
download_file_hashes: dict = {}


def write_download_hash():
    global new_file_hash, download_file_hashes
    if len(new_file_hash) == 0:
        return
    # Merge download_file_hashes to new_file_hash, if key exists do not cover
    # This preserves existing hashes while adding new ones
    download_file_hashes.update(new_file_hash)
    with open(hash_json_path, "w") as f:
        json.dump(download_file_hashes, f, indent=0)
        print_success('file_hash.json dumped.')


def download_packages():
    global new_file_hash, download_file_hashes
    download_path.mkdir(parents=True, exist_ok=True)
    address = RBC_SDK_ADDRESS
    lc_address = LC_SDK_ADDRESS
    downloads = {
        CLANGCXX_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / CLANGCXX_NAME,
                      tool_path / 'clangcxx_compiler']
        },
        CLANGD_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / CLANGD_NAME,
                      tool_path / 'clangd']
        },
        OIDN_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / OIDN_NAME,
                      download_path / 'oidn']
        },
        RENDER_RESOURCE_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / RENDER_RESOURCE_NAME,
                      download_path / 'render_resources']
        },
    }
    if LC_DX_SDK:
        downloads[LC_DX_SDK] = {
            "address": lc_address,
            "path": download_path,
            "unzip": [
                download_path / LC_DX_SDK,
                download_path / "dx_sdk"
            ]
        }

    if hash_json_path.exists():
        with open(hash_json_path, "r") as f:
            download_file_hashes = json.load(f)

    def download_file(file: str, map: dict):
        global new_file_hash, download_file_hashes
        dst_path = map["path"] / file
        _curr_hash = None

        def get_curr_path():
            nonlocal _curr_hash
            if _curr_hash:
                return _curr_hash
            _curr_hash = compute_hash(dst_path)
            return _curr_hash

        def unzip():
            nonlocal map
            unzip = map.get('unzip')
            if not unzip:
                return
            if not is_empty_folder(str(unzip[1])):
                last_hash = download_file_hashes.get(file)
                if last_hash and last_hash == get_curr_path():
                    print_debug(f"{file} skip extracting.")
                    return
            new_file_hash[file] = get_curr_path()
            unzip_dir(unzip[0], unzip[1])

        if os.path.exists(dst_path):
            print_debug(f"'{dst_path}' exists, skip download.")
            unzip()
            return
        print_info(f"Downloading '{dst_path}'...")
        response = requests.get(map["address"] + file)
        response.raise_for_status()
        # check if parent directory exists, else mkdir -p
        if not os.path.exists(os.path.dirname(dst_path)):
            os.makedirs(os.path.dirname(dst_path), exist_ok=True)

        with open(dst_path, "wb") as f:
            f.write(response.content)
        print_success(f"Download '{dst_path}' successfully!")
        unzip()

    executor = ThreadPoolExecutor(max_workers=8)
    futures1 = [
        executor.submit(download_file, name, downloads[name]) for name in downloads
    ]
    # unzip render resource
    return executor, futures1


def run_git_tasks():
    # Define tasks
    tasks = GIT_TASKS
    # Group by stages based on dependencies
    stage1 = [k for k, v in tasks.items() if not v["deps"]]
    stage2 = [k for k, v in tasks.items() if any(
        d in stage1 for d in v["deps"])]
    stage3 = [k for k, v in tasks.items() if any(
        d in stage2 for d in v["deps"])]

    def run_task(name):
        t = tasks[name]
        git_clone_or_pull(t["url"], t["subdir"], t["branch"])

    # Use ThreadPoolExecutor to run tasks in parallel
    with ThreadPoolExecutor(max_workers=8) as executor:
        # Stage 1
        futures1 = [executor.submit(run_task, name) for name in stage1]
        wait(futures1)
        for f in futures1:
            f.result()  # Raise exceptions if any

        # Stage 2
        futures2 = [executor.submit(run_task, name) for name in stage2]
        wait(futures2)
        for f in futures2:
            f.result()

        # Stage 3
        futures3 = [executor.submit(run_task, name) for name in stage3]
        wait(futures3)
        for f in futures3:
            f.result()


def run_package_download():
    download_executor, download_future = download_packages()
    wait(download_future)
    write_download_hash()
    for f in download_future:
        f.result()  # Raise exceptions if any


def _run_prepare():
    # ------------------------------ git ------------------------------
    print_warning("Git-clone and git-pull? (y/n)")
    try:
        clone_lc = input().strip()
    except EOFError:
        clone_lc = "n"

    if clone_lc.lower() == "y":
        run_git_tasks()

    lc_path = os.path.join(PROJECT_ROOT, "thirdparty/LuisaCompute")
    if is_empty_folder(lc_path):
        print_error("LuisaCompute not installed.")
        sys.exit(1)
    print_warning("Download package? (y/n)")
    try:
        download_package = input().strip()
    except EOFError:
        download_package = "n"

    if download_package.lower() == "y":
        run_package_download()

    # ------------------------------ llvm/options ------------------------------
    # We skip the builddir variable as it's dead code in the Lua source provided.

    print_warning("Write options? (y/n)")
    try:
        write_opt = input().strip()
    except EOFError:
        write_opt = "n"

    if write_opt.lower() == "y":
        # Find python path
        # Read python path from .venv/pyvenv.cfg

        pyvenv_cfg_path = os.path.join(PROJECT_ROOT, ".venv", "pyvenv.cfg")
        py_path = None
        if os.path.exists(pyvenv_cfg_path):
            with open(pyvenv_cfg_path, "r") as f:
                for line in f:
                    if line.startswith("home = "):
                        py_path = line.split("=", 1)[1].strip()
                        break

        # Construct dictionary for JSON
        options = {}
        # options["rbc_py_toolchain"] = PYD_TOOLCHAIN

        if py_path:

            def to_slash(p):
                return p.replace("\\", "/")

            options["lc_py_include"] = to_slash(
                os.path.join(py_path, "include"))
            options["rbc_py_bin"] = to_slash(py_path)

            lc_py_linkdir = os.path.join(py_path, "libs")
            options["lc_py_linkdir"] = to_slash(lc_py_linkdir)

            # Find libs starting with 'python'
            files = []
            if os.path.exists(lc_py_linkdir):
                for f in os.listdir(lc_py_linkdir):
                    if f.lower().endswith(".lib") and f.lower().startswith("python"):
                        files.append(f)

            if files:
                options["lc_py_libs"] = ";".join(files) + ";"

        # Write to xmake/options.json
        opt_json_path = rel("xmake/options.json")
        print_success(f"Write Options to {opt_json_path}")
        with open(opt_json_path, "w") as f:
            json.dump(options, f, indent=4)
        # Write to xmake/options.lua
        lua_sentence = f"""set_config('toolchain', '{XMAKE_GLOBAL_TOOLCHAIN}')
"""
        opt_lua_path = rel("xmake/options.lua")
        print_success(f"Write Options to {opt_lua_path}")
        with open(opt_lua_path, "w") as f:
            f.write(lua_sentence)
        write_shader_compile_cmd()

    # ------------------------------ Clean Up ------------------------------
    # Cleanup previous generated code to prevent disturbation
    # iterate all "generated" directories in the rbc/
    print_warning("clean up previous generated code? (y/n)")
    try:
        clean_up = input().strip()
    except EOFError:
        clean_up = "n"
    if clean_up.lower() == "y":
        clean_up_generated_code()
def prepare():
    try:
        _run_prepare()
    except KeyboardInterrupt as e:
        print_warning('quit.')
    except EOFError as e:
        print_warning('eof.')

def clean_up_generated_code():
    for generated_dir in Path("rbc").glob("**/generated"):
        if generated_dir.is_dir():
            shutil.rmtree(generated_dir)
            print_success(f"Cleaned up {generated_dir}")
        else:
            print_error(f"{generated_dir} is not a directory")
            sys.exit(1)

def _parse_version(version_str: str) -> tuple[int, int, int]:
    """Parse version string to major, minor, patch tuple."""
    parts = version_str.strip().split('.')
    major = int(parts[0]) if len(parts) > 0 else 0
    minor = int(parts[1]) if len(parts) > 1 else 0
    patch = int(parts[2]) if len(parts) > 2 else 0
    return major, minor, patch


def _generate_cxx_version_header(version: str, output_path: Path) -> None:
    """Generate C++ version header file."""
    major, minor, patch = _parse_version(version)
    
    content = f"""#pragma once

// Auto-generated by generate(). Do not edit manually.
// Source: pyproject.toml version = "{version}"

#define RBC_VERSION_MAJOR {major}
#define RBC_VERSION_MINOR {minor}
#define RBC_VERSION_PATCH {patch}

#define RBC_VERSION (RBC_VERSION_MAJOR * 10000 + RBC_VERSION_MINOR * 100 + RBC_VERSION_PATCH)
#define RBC_VERSION_STRING "{version}"
"""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print_success(f"Generated C++ version header: {output_path}")


def _update_python_version(version: str, init_file_path: Path) -> None:
    """Update __version__ in Python __init__.py file."""
    if not init_file_path.exists():
        print_error(f"Python init file not found: {init_file_path}")
        return
    
    with open(init_file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Replace __version__ = "x.x.x" or __version__ = 'x.x.x'
    import re
    new_content = re.sub(
        r'(__version__\s*=\s*)["\'][^"\']*["\']',
        f'\\1"{version}"',
        content
    )
    
    if new_content != content:
        with open(init_file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print_success(f"Updated Python __version__: {init_file_path}")
    else:
        print_debug(f"Python __version__ already up-to-date: {init_file_path}")


def _read_pyproject_version(pyproject_path: Path) -> str | None:
    """Read version from pyproject.toml."""
    if not pyproject_path.exists():
        return None
    
    with open(pyproject_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    import re
    match = re.search(r'^version\s*=\s*["\']([^"\']+)["\']', content, re.MULTILINE)
    if match:
        return match.group(1)
    return None


def generate_version_files() -> None:
    """Generate version files for C++ and Python from pyproject.toml."""
    pyproject_path = Path(PROJECT_ROOT) / "pyproject.toml"
    version = _read_pyproject_version(pyproject_path)
    
    if not version:
        print_error(f"Could not read version from {pyproject_path}")
        return
    
    print_info(f"Generating version files for version: {version}")
    
    # Generate C++ version header
    cxx_header_path = rel("rbc/core/include/rbc_core/generated/version.h")
    _generate_cxx_version_header(version, cxx_header_path)
    
    # Update Python __version__
    py_init_path = rel("src/robocute/__init__.py")
    _update_python_version(version, py_init_path)


def generate():
    import scripts.generate
    from scripts.generate import generate_registered

    generate_registered()

    # Generate version files from pyproject.toml
    generate_version_files()

def pre_pack():
    """
    Pre-packaging script: Copy C++ build artifacts (dll, pyd, bytes) and shader builds
    to src/robocute/rbc_ext/_C for packaging. Optionally generate stub files.

    Usage: uv run pre-pack <mode> <build_stubgen>

    Args:
        mode: Build mode (debug, release, releasedbg). Defaults to 'release'.
        build_stubgen: Stub generator to use ('uv' for uvx, or None to skip).
    """
    import argparse

    parser = argparse.ArgumentParser(
        description='Pre-packaging script for RoboCute')
    parser.add_argument('mode', nargs='?', default='release',
                        choices=['debug', 'release', 'releasedbg'],
                        help='Build mode (default: release)')

    parser.add_argument('build_stubgen', nargs='?', default="uv",
                        help='Stub generator to use ("uv" for uvx, or omit to skip)')

    args = parser.parse_args()

    mode = args.mode
    build_stubgen = args.build_stubgen

    # Determine target directory
    target_dir = rel(f"build/{PLATFORM}/{ARCH}/{mode}")
    ext_path = rel("src/robocute/rbc_ext/_C")

    print("=" * 60)
    print("RoboCute Pre-Pack")
    print("=" * 60)
    print(f"Mode: {mode}")
    print(f"Source: {target_dir}")
    print(f"Destination: {ext_path}")
    print()

    # Check if source directory exists
    if not target_dir.exists():
        print_error(f"Build directory not found: {target_dir}")
        print("Please build the project first with xmake.")
        sys.exit(1)

    # Ensure destination directory exists
    ext_path.mkdir(parents=True, exist_ok=True)

    # Copy files by extension
    extensions = ['dll', 'pyd', 'bytes']
    copied_files = []

    for ext in extensions:
        pattern = f"*.{ext}"
        files = list(target_dir.glob(pattern))
        for src_file in files:
            dst_file = ext_path / src_file.name
            try:
                shutil.copy2(src_file, dst_file)
                copied_files.append(src_file.name)
            except Exception as e:
                print_error(f"Failed to copy {src_file.name}: {e}")

    if copied_files:
        print_success(f"Copied {len(copied_files)} files:")
        for f in copied_files[:10]:  # Show first 10
            print(f"  - {f}")
        if len(copied_files) > 10:
            print(f"  ... and {len(copied_files) - 10} more")
    else:
        print_warning("No dll/pyd/bytes files found to copy.")
    print()

    # Copy shader build directories
    shader_names = ['shader_build_dx', 'shader_build_vk']
    shader_base = rel(f"build/{PLATFORM}/{ARCH}")
    copied_shaders = []

    for shader_name in shader_names:
        shader_dir = shader_base / shader_name
        if shader_dir.exists() and shader_dir.is_dir():
            dst_shader_dir = ext_path / shader_name

            # Remove existing directory if present
            if dst_shader_dir.exists():
                shutil.rmtree(dst_shader_dir)

            try:
                shutil.copytree(shader_dir, dst_shader_dir)
                copied_shaders.append(shader_name)
            except Exception as e:
                print_error(f"Failed to copy {shader_name}: {e}")

    if copied_shaders:
        print_success(f"Copied shader builds: {', '.join(copied_shaders)}")
    else:
        print_warning("No shader build directories found.")
    print()

    # Generate stub files if requested
    if build_stubgen:
        print("Generating stub files...")
        os.environ['PYTHONPATH'] = str(ext_path)

        modules = ['rbc_ext_c', 'lcapi_c']

        for module in modules:
            # Check if .pyd file exists before generating stub
            pyd_file = ext_path / f"{module}.pyd"
            if not pyd_file.exists():
                print_warning(
                    f"Skipping stub for {module}: {pyd_file.name} not found")
                continue

            try:
                if build_stubgen == 'uv':
                    subprocess.run(
                        ['uvx', 'pybind11-stubgen', module,
                            f'--output-dir={ext_path}'],
                        check=True,
                        capture_output=True,
                        text=True
                    )
                else:
                    subprocess.run(
                        ['pybind11-stubgen', module,
                            f'--output-dir={ext_path}'],
                        check=True,
                        capture_output=True,
                        text=True
                    )
                print_success(f"Generated stub for {module}")
            except subprocess.CalledProcessError as e:
                print_error(f"Failed to generate stub for {module}")
                if e.stderr:
                    print(f"  Error: {e.stderr}")
            except FileNotFoundError:
                print_error(
                    f"pybind11-stubgen not found. Please install it or use 'uv' option.")
        print()

    print("=" * 60)
    print("Pre-pack completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    # main()
    print(PROJECT_ROOT)
