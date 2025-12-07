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
from multiprocessing import Process
from scripts.generate import GENERATION_TASKS
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
from scripts.utils import is_empty_folder
from mypy import stubgen
import rbc_meta.utils.codegen_util as codegen_util
import rbc_meta.utils.codegen_util as ut
from rbc_meta.utils_next.codegen import (
    cpp_interface_gen,
    cpp_impl_gen,
)


def get_project_root():
    # Assumes this script is located in <root>/src/srcripts/scripts.py
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = Path(script_dir).parent.parent
    print(f"Checking ProjectRoot {project_root}")
    return project_root


PROJECT_ROOT = get_project_root()


def rel(path):
    global PROJECT_ROOT
    return Path(PROJECT_ROOT) / path


def find_process_path(process_name):
    # mimicks finding executable in PATH
    path = shutil.which(process_name)
    if path:
        return os.path.dirname(path)
    return None


def write_shader_compile_cmd():
    clangcxx_dir = rel(CLANGCXX_PATH)
    shader_dir = rel(SHADER_PATH)
    in_dir = shader_dir / "src"
    # TODO: different platform
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
        out_dir = Path(PROJECT_ROOT) / f"build/{PLATFORM}/{ARCH}/shader_build_{backend}"
        f = open(shader_dir / f"{backend}_compile.cmd", "w")
        f.write("@echo off\n" + build_cmd() + f" -backend={backend}")
        f.close()

        f = open(shader_dir / f"{backend}_clean_compile.cmd", "w")
        f.write("@echo off\n" + build_cmd() + f" -backend={backend}" + " -rebuild")
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
        print(f"pulling {git_address} to {abs_subdir}")
    else:
        # Pull
        args = ["git", "-C", abs_subdir, "pull"]
        if branch:
            args.extend(["origin", branch])
        print(f"pulling {git_address} to {abs_subdir}")

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
        print(f"git clone {git_address} error.")
        sys.exit(1)


def download_packages():
    download_path = Path(PROJECT_ROOT) / "build/download"
    download_path.mkdir(parents=True, exist_ok=True)
    address = RBC_SDK_ADDRESS
    lc_address = LC_SDK_ADDRESS
    lc_path = rel("thirdparty/LuisaCompute/SDKs")
    downloads = {
        CLANGCXX_NAME: {
            "address": address,
            "path": download_path,
        },
        CLANGD_NAME: {
            "address": address,
            "path": download_path,
        },
        OIDN_NAME: {
            "address": address,
            "path": download_path,
        },
        RENDER_RESOURCE_NAME: {
            "address": address,
            "path": download_path,
        },
        LC_DX_SDK: {
            "address": lc_address,
            "path": lc_path,
        },
    }
    lua_file = f'''clangd_filename = "{CLANGD_NAME}"
clangcxx_filename = "{CLANGCXX_NAME}"
oidn = "{OIDN_NAME}"'''
    codegen_util._write_string_to(lua_file, PROJECT_ROOT / "rbc/generate.lua")

    def download_file(file: str, map: dict):
        print(file)
        dst_path = str(map["path"] / file)
        if os.path.exists(dst_path):
            print(f"'{dst_path}' exists, skip download.")
            return
        print(f"Downloading '{dst_path}'...")
        response = requests.get(map["address"] + file)
        response.raise_for_status()
        with open(dst_path, "wb") as f:
            f.write(response.content)
        print(f"Download '{dst_path}' successfully!")

    executor = ThreadPoolExecutor(max_workers=8)
    futures1 = [
        executor.submit(download_file, name, downloads[name]) for name in downloads
    ]
    return executor, futures1


def run_git_tasks():
    # Define tasks
    tasks = GIT_TASKS
    # Group by stages based on dependencies
    stage1 = [k for k, v in tasks.items() if not v["deps"]]
    stage2 = [k for k, v in tasks.items() if any(d in stage1 for d in v["deps"])]
    stage3 = [k for k, v in tasks.items() if any(d in stage2 for d in v["deps"])]

    def run_task(name):
        t = tasks[name]
        git_clone_or_pull(t["url"], t["subdir"], t["branch"])

    print("git...")
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


def prepare():
    # ------------------------------ git ------------------------------
    print("Download, git-clone and git-pull? (y/n)")
    try:
        clone_lc = input().strip()
    except EOFError:
        clone_lc = "n"

    if clone_lc.lower() == "y":
        download_executor, download_future = download_packages()
        run_git_tasks()
        wait(download_future)
        for f in download_future:
            f.result()  # Raise exceptions if any
        del download_executor

    lc_path = os.path.join(PROJECT_ROOT, "thirdparty/LuisaCompute")
    if is_empty_folder(lc_path):
        print("LuisaCompute not installed.")
        sys.exit(1)

    # ------------------------------ llvm/options ------------------------------
    # We skip the builddir variable as it's dead code in the Lua source provided.

    print("Write options? (y/n)")
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

            options["lc_py_include"] = to_slash(os.path.join(py_path, "include"))
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
        print(f"Write Options to {opt_json_path}")
        with open(opt_json_path, "w") as f:
            json.dump(options, f, indent=4)
        # Write to xmake/options.lua
        lua_sentence = f"""set_config('toolchain', '{XMAKE_GLOBAL_TOOLCHAIN}')
"""
        opt_lua_path = rel("xmake/options.lua")
        print(f"Write Options to {opt_lua_path}")
        with open(opt_lua_path, "w") as f:
            f.write(lua_sentence)
        write_shader_compile_cmd()

    # ------------------------------ Clean Up ------------------------------
    # Cleanup previous generated code to prevent disturbation
    # iterate all "generated" directories in the rbc/
    print("Clean up previous generated code? (y/n)")
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
            print(f"Cleaned up {generated_dir}")
        else:
            print(f"{generated_dir} is not a directory")
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
            print(f"[{module_name}] Starting generation...")
            func(*args)
            print(f"[{module_name}] Completed successfully.")
        else:
            print(f"[{module_name}] Error: Function '{function_name}' not found.")
            sys.exit(1)

    except ImportError as e:
        print(f"[{module_name}] Error: Failed to import module. {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[{module_name}] Error: An unexpected error occurred. {e}")
        sys.exit(1)


def generate():
    """
    Main execution function. Sets up paths and spawns parallel processes for generation.
    """
    start_time = time.time()
    print(f"Starting code generation for {len(GENERATION_TASKS)} modules...")

    import rbc_meta.types

    target_modules = ["runtime"]
    include = """#include<luisa/runtime/rhi/pixel.h>"""
    header_path = Path(
        "rbc/runtime/include/rbc_runtime/generated/resource_meta.new.hpp"
    ).resolve()
    cpp_path = Path("rbc/runtime/src/runtime/generated/resource_meta.new.cpp").resolve()
    ut.codegen_to(header_path)(cpp_interface_gen, target_modules, include)
    include = "#include <rbc_runtime/generated/resource_meta.hpp>"
    ut.codegen_to(cpp_path)(cpp_impl_gen, target_modules, include)

    target_modules = ["world"]
    header_path = Path(
        "rbc/world/include/rbc_world/generated/resource_type.new.hpp"
    ).resolve()
    cpp_path = Path("rbc/world/src/generated/resource_type.new.cpp").resolve()
    ut.codegen_to(header_path)(cpp_interface_gen, target_modules)
    ut.codegen_to(cpp_path)(cpp_impl_gen, target_modules)

    ut.codegen_to(header_path / "server.new.hpp")(cpp_interface_gen)

    include = '#include "server.hpp"'
    ut.codegen_to(header_path / "server.new.cpp")(cpp_impl_gen, include)

    # processes = []
    # for module_name, function_name, *args in GENERATION_TASKS:
    #     p = Process(
    #         target=run_generation_task, args=(module_name, function_name, *args)
    #     )
    #     p.start()
    #     processes.append(p)

    # # Wait for all processes to complete
    exit_code = 0
    # for p in processes:
    #     p.join()
    #     if p.exitcode != 0:
    #         exit_code = 1
    #         print(f"Process for task {p.name} failed with exit code {p.exitcode}")

    duration = time.time() - start_time
    print(f"Code generation finished in {duration:.2f} seconds.")
    sys.exit(exit_code)


def generate_stub_impl(module_name: str, pyd_dir: Path, output_dir: Path):
    """
    Generate .pyi stub file for a given .pyd module.

    Args:
        module_name: Name of the module (without .pyd extension)
        pyd_dir: Directory containing the .pyd file
        output_dir: Directory where .pyi file should be generated
    """

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
        print(f"  ✓ Module {module_name} imported successfully")
    except ImportError as e:
        print(f"  ✗ Error: Cannot import module {module_name}")
        print(f"    {e}")
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
        print(f"Error generating stubs: {e}")
        sys.exit(1)

    print(f"Stub generated for module: {module_name}")
    print(f"  Output directory: {output_dir.resolve()}")


def generate_stub():
    start_time = time.time()
    for task in GENERATE_SUB_TASKS:
        generate_stub_impl(
            task["module_name"], rel(task["pyd_dir"]), rel(task["stub_output"])
        )
    duration = time.time() - start_time
    print(f"Stub generation finished in {duration:.2f} seconds.")


if __name__ == "__main__":
    # main()
    print(PROJECT_ROOT)
