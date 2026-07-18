"""
Install script that builds the project and copies artifacts to the extension directory.

This script handles:
1. Building the project using xmake
2. Copying DLLs, PYDs, shader files to the extension directory
3. Optionally generating Python stubs
"""

import argparse
import os
import sys
import shutil
import platform
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Callable, Optional, Sequence


# Import from src/scripts
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))
from rbc_build.utils import print_success, print_error, print_warning, print_info, print_debug
from rbc_build.prepare import PLATFORM, ARCH
from rbc_build.artifact_publish import (
    install_build_artifacts as publish_build_artifacts,
)
from rbc_build.shader_variants import (
    ShaderVariantError,
    _atomic_replace_directory,
    install_verified_shader_root,
    load_config,
)


def get_project_dir() -> Path:
    """Get the project root directory."""
    return Path(__file__).parent.parent.resolve()


def get_target_dir(mode: str) -> Path:
    """
    Get the build target directory based on platform, architecture and mode.

    Args:
        mode: Build mode (debug, release, releasedbg)

    Returns:
        Path to the target directory
    """
    project_dir = get_project_dir()
    return project_dir / "build" / PLATFORM / ARCH / mode


def run_xmake_build(rebuild: bool = False) -> bool:
    """
    Run xmake build to compile the project.

    Args:
        rebuild: If True, force rebuild using `xmake -r`

    Returns:
        True if build succeeded, False otherwise
    """
    project_dir = get_project_dir()

    # Build command: xmake or xmake -r
    cmd = ["xmake", "-r"] if rebuild else ["xmake"]

    print_info(f"Running build command: {' '.join(cmd)}")
    print_info(f"Working directory: {project_dir}")

    try:
        result = subprocess.run(
            cmd,
            cwd=project_dir,
            capture_output=False,  # Show output in real-time
            text=True
        )

        if result.returncode != 0:
            return False

        print_success("Build completed successfully!")
        return True

    except FileNotFoundError:
        print_error("Error: 'xmake' command not found. Please install xmake.")
        print_info("Installation instructions: https://xmake.io/#/guide/installation")
        return False

    except Exception as e:
        print_error(f"Unexpected error during build: {e}")
        return False


def copy_if_different(src: Path, dst: Path) -> bool:
    """
    Copy file from src to dst only if they are different or dst doesn't exist.

    Args:
        src: Source file path
        dst: Destination file path

    Returns:
        True if file was copied, False otherwise
    """
    # Ensure destination directory exists
    dst.parent.mkdir(parents=True, exist_ok=True)

    # Check if destination exists and compare with source
    if dst.exists():
        # Compare file sizes first (quick check)
        if src.stat().st_size != dst.stat().st_size:
            shutil.copy2(src, dst)
            return True

        # Compare modification times
        if src.stat().st_mtime != dst.stat().st_mtime:
            shutil.copy2(src, dst)
            return True

        # Files appear to be the same
        return False
    else:
        # Destination doesn't exist, copy the file
        shutil.copy2(src, dst)
        return True


def copy_files_with_pattern(target_dir: Path, ext_path: Path, pattern: str) -> int:
    """
    Copy files matching pattern from target_dir to ext_path.

    Args:
        target_dir: Source directory to search for files
        ext_path: Destination directory
        pattern: File pattern to match (e.g., '*.dll')

    Returns:
        Number of files copied
    """
    count = 0
    for src_file in target_dir.glob(pattern):
        dst_file = ext_path / src_file.name
        if copy_if_different(src_file, dst_file):
            count += 1
    return count


def copy_shader(shader_name: str, target_dir: Path, ext_path: Path) -> int:
    """
    Copy shader build directory to the extension path.

    Args:
        shader_name: Name of the shader (e.g., 'dx', 'vk')
        target_dir: Base target directory
        ext_path: Destination extension path

    Returns:
        Number of items copied
    """
    # Shader path is at parent of target_dir: ../shader_build_<name>
    shader_src = target_dir.parent / f"shader_build_{shader_name}"
    shader_dst = ext_path / f"shader_build_{shader_name}"
    install_verified_shader_root(shader_src, shader_dst, shader_name)

    # Count items copied
    count = sum(1 for _ in shader_dst.rglob("*") if _.is_file())
    return count


def install_build_artifacts(
    target_dir: Path,
    ext_path: Path,
    shader_types: Sequence[str],
    *,
    build_stubgen: Optional[str] = None,
    host_output: Optional[Path] = None,
    publisher: Callable[[Path, Path], None] = _atomic_replace_directory,
) -> int:
    """Publish through the shared generation-checked artifact installer."""
    resolved_host_output = (
        host_output
        if host_output is not None
        else get_project_dir() / "rbc" / "shader" / "host"
    )
    prepare_staged = (
        (lambda staged: run_stubgen(staged, build_stubgen))
        if build_stubgen
        else None
    )
    return publish_build_artifacts(
        target_dir,
        ext_path,
        shader_types,
        host_output=resolved_host_output,
        prepare_staged=prepare_staged,
        publisher=publisher,
    )


def run_jobs(jobs: list) -> None:
    """
    Execute jobs concurrently using a thread pool.

    Args:
        jobs: List of tuples (name, function, args)
    """
    with ThreadPoolExecutor(max_workers=len(jobs)) as executor:
        # Submit all jobs
        future_to_job = {
            executor.submit(job[1], *job[2]): job for job in jobs
        }

        # Wait for completion and handle results
        for future in as_completed(future_to_job):
            name = future_to_job[future][0]
            try:
                future.result()
            except Exception as e:
                print_error(f"Job '{name}' failed: {e}")
                raise


def run_stubgen(ext_path: Path, build_stubgen: str) -> None:
    """
    Run pybind11-stubgen to generate Python stubs.

    Args:
        ext_path: Path to the extension directory
        build_stubgen: Stub generator type ('uv' for uvx, or other value)
    """
    # Set PYTHONPATH environment variable
    env = os.environ.copy()
    env["PYTHONPATH"] = str(ext_path)

    # Modules to generate stubs for
    modules = ["rbc_ext_c", "lcapi_c"]

    for module in modules:
        # Check if .pyd file exists before generating stub
        pyd_file = ext_path / f"{module}.pyd"
        if not pyd_file.exists():
            print_warning(f"Skipping stub for {module}: {pyd_file.name} not found")
            continue

        try:
            if build_stubgen == "uv":
                cmd = ["uvx", "pybind11-stubgen", module, f"--output-dir={ext_path}"]
            else:
                cmd = ["pybind11-stubgen", module, f"--output-dir={ext_path}"]

            print_info(f"Running: {' '.join(cmd)}")
            subprocess.run(cmd, env=env, check=True, capture_output=True, text=True)
            print_success(f"Generated stub for {module}")

        except subprocess.CalledProcessError as e:
            print_error(f"Failed to generate stub for {module}")
            if e.stderr:
                print(f"  Error: {e.stderr}")
        except FileNotFoundError:
            print_error(
                f"pybind11-stubgen not found. Please install it or use 'uv' option."
            )


def main(mode: Optional[str] = None, build_stubgen: Optional[str] = None, rebuild: bool = False) -> int:
    """
    Main install function.

    Args:
        mode: Build mode, defaults to 'release' if not provided
        build_stubgen: If provided, generate stubs. Use 'uv' to use uvx tool.
        rebuild: If True, force rebuild using `xmake -r`

    Returns:
        Exit code (0 for success, 1 for failure)
    """
    # Default mode is 'release'
    if not mode:
        mode = "release"

    # Validate mode
    valid_modes = ["debug", "release", "releasedbg"]
    if mode not in valid_modes:
        print_error(f"Invalid mode: {mode}. Must be one of: {', '.join(valid_modes)}")
        return 1

    # Get project directory
    project_dir = get_project_dir()

    # Define extension path where files will be copied
    ext_path = project_dir / "src" / "robocute" / "rbc_ext" / "_C"

    # Create extension directory if it doesn't exist
    ext_path.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("RoboCute Install")
    print("=" * 60)
    print(f"Mode: {mode}")
    print(f"Extension path: {ext_path}")
    print()

    # Step 1: Run xmake build
    print_debug("Step 1: Building project with xmake...")
    if not run_xmake_build(rebuild):
        print_error("Build failed. Installation aborted.")
        return 1
    print()

    # Get target build directory
    target_dir = get_target_dir(mode)
    print_info(f"Target directory: {target_dir}")

    if not target_dir.exists():
        print_error(f"Target directory does not exist: {target_dir}")
        return 1

    # Step 2: Publish binaries and shaders as one verified generation.
    print_debug("Step 2: Publishing build artifacts...")
    shader_types = list(
        load_config(
            project_dir / "rbc" / "shader" / "shader_variants.json"
        ).backends
    )
    try:
        copied_files = install_build_artifacts(
            target_dir,
            ext_path,
            shader_types,
            build_stubgen=build_stubgen,
        )
    except (OSError, ShaderVariantError) as error:
        print_error(f"Artifact publication failed: {error}")
        return 1
    print_success(
        f"Published {copied_files} binary/resource files and "
        f"{len(shader_types)} shader roots."
    )
    print()

    print("=" * 60)
    print("Install completed successfully!")
    print("=" * 60)

    return 0


if __name__ == "__main__":
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description="Build and install RoboCute extension.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  uv run scripts/build_and_copy.py debug
  uv run scripts/build_and_copy.py release uv
  uv run scripts/build_and_copy.py -r debug
  uv run scripts/build_and_copy.py --rebuild --mode releasedbg
        """
    )

    parser.add_argument(
        "mode",
        nargs="?",
        default="release",
        choices=["debug", "release", "releasedbg"],
        help="Build mode (default: release)"
    )

    parser.add_argument(
        "-m", "--mode",
        dest="mode_flag",
        default=None,
        choices=["debug", "release", "releasedbg"],
        help="Build mode (overrides positional mode argument)"
    )

    parser.add_argument(
        "-r", "--rebuild",
        action="store_true",
        help="Force rebuild (clean and rebuild the project)"
    )

    parser.add_argument(
        "--stubgen",
        nargs="?",
        const="1",
        default=None,
        help="Generate Python stubs. Use 'uv' (default) or specify stub generator type"
    )

    args = parser.parse_args()

    # Determine mode: --mode flag takes priority over positional argument
    mode = args.mode_flag if args.mode_flag else args.mode

    sys.exit(main(mode, args.stubgen, args.rebuild))
