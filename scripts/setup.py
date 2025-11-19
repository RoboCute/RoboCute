import os
import sys
import subprocess
import json
import shutil
from concurrent.futures import ThreadPoolExecutor, wait


def get_project_root():
    # Assumes this script is located in <root>/scripts/setup.py
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(script_dir)


PROJECT_ROOT = get_project_root()


def is_empty_folder(dir_path):
    if os.path.exists(dir_path) and not os.path.isfile(dir_path):
        # Check if directory is empty
        if not os.listdir(dir_path):
            return True
        return False
    else:
        # Treat non-existent directory as empty (ready for cloning)
        return True


def find_process_path(process_name):
    # mimicks finding executable in PATH
    path = shutil.which(process_name)
    if path:
        return os.path.dirname(path)
    return None


def git_clone_or_pull(git_address, subdir, branch=None):
    abs_subdir = os.path.join(PROJECT_ROOT, subdir)

    # Normalize path separators for display
    display_subdir = subdir.replace("\\", "/")

    args = []
    if is_empty_folder(abs_subdir):
        # Clone
        args = ["git", "clone", git_address]
        if branch:
            args.extend(["-b", branch])
        args.append(abs_subdir)
        # print(f"pulling {git_address}") # Matching Lua output exactly? Lua says "pulling..." for both.
        # But let's use the Lua string for consistency if possible, or just informative.
        # Lua: print("pulling " .. git_address)
        print(f"pulling {git_address}")
    else:
        # Pull
        args = ["git", "-C", abs_subdir, "pull"]
        if branch:
            args.extend(["origin", branch])
        print(f"pulling {git_address}")

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


def run_git_tasks():
    # Define tasks
    tasks = {
        "nanobind": {
            "subdir": "thirdparty/nanobind",
            "url": "https://github.com/LuisaGroup/nanobind.git",
            "branch": None,
            "deps": [],
        },
        "robinmap": {
            "subdir": "thirdparty/nanobind/ext/robin_map",
            "url": "https://github.com/Tessil/robin-map.git",
            "branch": None,
            "deps": ["nanobind"],
        },
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
        "magic_enum": {
            "subdir": "thirdparty/LuisaCompute/src/ext/magic_enum",
            "url": "https://github.com/LuisaGroup/magic_enum",
            "branch": None,
            "deps": ["lc"],
        },
        "marl": {
            "subdir": "thirdparty/LuisaCompute/src/ext/marl",
            "url": "= https://github.com/LuisaGroup/marl.git",
            "branch": None,
            "deps": ["lc"],
        },
        "imgui": {
            "subdir": "thirdparty/LuisaCompute/src/ext/imgui",
            "url": "https://github.com/ocornut/imgui.git",
            "branch": "docking",
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
    }

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


def main():
    # ------------------------------ git ------------------------------
    print("Clone submod? (y/n)")
    try:
        clone_lc = input().strip()
    except EOFError:
        clone_lc = "n"

    if clone_lc.lower() == "y":
        run_git_tasks()

    lc_path = os.path.join(PROJECT_ROOT, "thirdparty/LuisaCompute")
    if is_empty_folder(lc_path):
        print("LuisaCompute not installed.")
        sys.exit(1)

    # ------------------------------ llvm/options ------------------------------
    # We skip the builddir variable as it's dead code in the Lua source provided.

    print("Write options.lua? (y/n)")
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
        options["lc_toolchain"] = "clang-cl"

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

        # Write to scripts/options.json
        opt_json_path = os.path.join(PROJECT_ROOT, "scripts/options.json")
        with open(opt_json_path, "w") as f:
            json.dump(options, f, indent=4)


if __name__ == "__main__":
    main()
