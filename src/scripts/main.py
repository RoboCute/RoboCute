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
from scripts.generate_stub import GENERATE_SUB_TASKS
from scripts.utils import is_empty_folder, get_project_root, rel, compute_hash, unzip_dir, print_success, print_error, print_warning, print_info, print_debug
from scripts.install import install_resources

import rbc_meta.utils.codegen_util as ut

from rbc_meta.utils.codegen import (
    cpp_interface_gen,
    cpp_impl_gen,
    pybind_codegen,
    py_interface_gen,
)


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
        args = ["git", "clone", git_address]
        if branch:
            args.extend(["-b", branch])
        args.append(abs_subdir)
        print_info(f"pulling {git_address} to {abs_subdir}")
    else:
        # Pull
        args = ["git", "-C", abs_subdir, "pull"]
        if branch:
            args.extend(["origin", branch])
        print_info(f"pulling {git_address} to {abs_subdir}")

    done = False
    for i in range(4):
        try:
            subprocess.check_call(
                args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
            done = True
            break
        except subprocess.CalledProcessError:
            # Retry logic handled by loop
            continue

    if not done:
        print_error(f"git clone {git_address} error.")
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
    lc_path = rel("thirdparty/LuisaCompute/SDKs")
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
        LC_DX_SDK: {
            "address": lc_address,
            "path": lc_path,
        },
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

def prepare():
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


def clean_up_generated_code():
    for generated_dir in Path("rbc").glob("**/generated"):
        if generated_dir.is_dir():
            shutil.rmtree(generated_dir)
            print_success(f"Cleaned up {generated_dir}")
        else:
            print_error(f"{generated_dir} is not a directory")
            sys.exit(1)


def run_generation_task(module_name, function_name, *args):
    """
    Imports a module and executes its specified entry point function.

    Args:
        module_name (str): The name of the module to import.
        function_name (str): The name of the function to call within the module.
    """
    try:
        # Dynamic import replaces the unsafe exec()
        module = importlib.import_module(module_name)

        if hasattr(module, function_name):
            func = getattr(module, function_name)
            print_info(f"[{module_name}] Starting generation...")
            func(*args)
            print_success(f"[{module_name}] Completed successfully.")
        else:
            print_error(
                f"[{module_name}] Error: Function '{function_name}' not found.")
            sys.exit(1)

    except ImportError as e:
        print_error(f"[{module_name}] Error: Failed to import module. {e}")
        sys.exit(1)
    except Exception as e:
        print_error(
            f"[{module_name}] Error: An unexpected error occurred. {e}")
        sys.exit(1)


def generate():
    import scripts.generate
    from scripts.generate import generate_registered

    generate_registered()


def generate_stub_impl(module_name: str, pyd_dir: Path, output_dir: Path):
    """
    Generate .pyi stub file for a given .pyd module.

    Args:
        module_name: Name of the module (without .pyd extension)
        pyd_dir: Directory containing the .pyd file
        output_dir: Directory where .pyi file should be generated
    """
    from mypy import stubgen

    # Add pyd directory to Python path
    pyd_dir_abs = pyd_dir.resolve()
    if str(pyd_dir_abs) not in sys.path:
        sys.path.insert(0, str(pyd_dir_abs))

    print(f"Generating stub for module: {module_name}")
    print(f"  .pyd location: {pyd_dir_abs}")
    print(f"  Output directory: {output_dir.resolve()}")

    # Verify module can be imported
    try:
        __import__(module_name)
        print_success(f"  ✓ Module {module_name} imported successfully")
    except ImportError as e:
        print_error(f"  ✗ Error: Cannot import module {module_name}")
        print_error(f"    {e}")
        sys.exit(1)

    options = stubgen.Options(
        pyversion=sys.version_info[:2],
        no_import=False,
        inspect=True,
        doc_dir="",
        search_path=[str(pyd_dir)],
        interpreter=sys.executable,
        parse_only=False,
        ignore_errors=False,
        include_private=False,
        output_dir=str(output_dir),
        modules=[module_name],
        packages=[],
        files=[],
        verbose=True,
        quiet=False,
        export_less=False,
        include_docstrings=False,
    )
    try:
        stubgen.generate_stubs(options)
    except Exception as e:
        print_error(f"Error generating stubs: {e}")
        sys.exit(1)

    print_success(f"Stub generated for module: {module_name}")
    print_success(f"  Output directory: {output_dir.resolve()}")


def generate_stub():
    start_time = time.time()
    for task in GENERATE_SUB_TASKS:
        generate_stub_impl(
            task["module_name"], rel(task["pyd_dir"]), rel(task["stub_output"])
        )
    duration = time.time() - start_time
    print_success(f"Stub generation finished in {duration:.2f} seconds.")


def install():
    """Install resources after build (test_scene and shader build results)."""
    install_resources()


if __name__ == "__main__":
    # main()
    print(PROJECT_ROOT)
