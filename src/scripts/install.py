"""
Install script for RoboCute Desktop project.
Handles post-build resource installation:
1. Extract test_scene archive to target directory
2. Extract render_resources archive to target directory
3. Copy shader build results to target directory parent
"""
import os
import sys
import shutil
import subprocess
from pathlib import Path
from scripts.utils import get_project_root, rel
from scripts.prepare import PLATFORM, ARCH, RENDER_RESOURCE_NAME

PROJECT_ROOT = get_project_root()

# Test scene version (should match rbc/tests/test_graphics/xmake.lua)
TEST_SCENE_VERSION = "v1.0.1"
TEST_SCENE_ARCHIVE = f"test_scene_{TEST_SCENE_VERSION}.7z"


def find_7z_executable():
    """Find 7z executable for extracting archives."""
    # Try common locations
    possible_paths = [
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
        shutil.which("7z"),
        shutil.which("7za"),
    ]
    
    for path in possible_paths:
        if path and os.path.exists(path):
            return path
    
    return None


def extract_7z(archive_path: Path, extract_to: Path):
    """Extract a 7z archive to the specified directory."""
    seven_zip = find_7z_executable()
    if not seven_zip:
        print("ERROR: 7z executable not found. Please install 7-Zip.")
        print("  Download from: https://www.7-zip.org/")
        sys.exit(1)
    
    extract_to.mkdir(parents=True, exist_ok=True)
    
    print(f"Extracting {archive_path.name} to {extract_to}...")
    try:
        subprocess.check_call(
            [seven_zip, "x", str(archive_path), f"-o{extract_to}", "-y"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE
        )
        print(f"✓ Successfully extracted {archive_path.name}")
    except subprocess.CalledProcessError:
        print(f"ERROR: Failed to extract {archive_path.name}")
        print(f"  Command: {seven_zip} x {archive_path} -o{extract_to} -y")
        sys.exit(1)


def find_cmake_target_dir():
    """Find cmake build target directory."""
    # Common cmake build directories
    possible_dirs = [
        rel("build_cmake/bin/Release"),
        rel("build_cmake/bin/RelWithDebInfo"),
        rel("build_cmake/bin/Debug"),
        rel("build_cmake/bin/MinSizeRel"),
    ]
    
    for target_dir in possible_dirs:
        if target_dir.exists() and any(target_dir.iterdir()):
            # Check if it contains executables or DLLs
            has_binaries = any(
                f.suffix in [".exe", ".dll", ".pyd"] for f in target_dir.iterdir()
            )
            if has_binaries:
                return target_dir
    
    return None


def find_xmake_target_dir():
    """Find xmake build target directory."""
    # xmake output: build/{platform}/{arch}/{mode}
    possible_modes = ["release", "debug", "releasedbg"]
    
    for mode in possible_modes:
        target_dir = rel(f"build/{PLATFORM}/{ARCH}/{mode}")
        if target_dir.exists() and any(target_dir.iterdir()):
            # Check if it contains executables or DLLs
            has_binaries = any(
                f.suffix in [".exe", ".dll", ".pyd"] for f in target_dir.iterdir()
            )
            if has_binaries:
                return target_dir
    
    return None


def find_target_dir():
    """Find the build target directory (cmake or xmake)."""
    # Try cmake first
    cmake_dir = find_cmake_target_dir()
    if cmake_dir:
        print(f"Found CMake build directory: {cmake_dir}")
        return cmake_dir, "cmake"
    
    # Try xmake
    xmake_dir = find_xmake_target_dir()
    if xmake_dir:
        print(f"Found xmake build directory: {xmake_dir}")
        return xmake_dir, "xmake"
    
    return None, None


def find_shader_build_dirs(build_system: str, target_dir: Path):
    """Find shader build directories based on build system."""
    shader_dirs = []
    found_paths = set()  # Track found paths to avoid duplicates
    
    if build_system == "cmake":
        # For cmake: shader_build_dx and shader_build_vk could be in:
        # 1. build_cmake/shader_build_dx (in build_cmake root)
        # 2. build_cmake/bin/shader_build_dx (in bin directory)
        possible_bases = [
            rel("build_cmake"),
            rel("build_cmake/bin"),
        ]
        for base_dir in possible_bases:
            for shader_name in ["shader_build_dx", "shader_build_vk"]:
                shader_dir = base_dir / shader_name
                shader_path_str = str(shader_dir.resolve())
                if shader_dir.exists() and shader_dir.is_dir() and shader_path_str not in found_paths:
                    shader_dirs.append(shader_dir)
                    found_paths.add(shader_path_str)
    else:
        # For xmake: shader_build_dx and shader_build_vk are in build/{platform}/{arch}
        # (same level as the mode directories)
        xmake_shader_base = rel(f"build/{PLATFORM}/{ARCH}")
        for shader_name in ["shader_build_dx", "shader_build_vk"]:
            shader_dir = xmake_shader_base / shader_name
            if shader_dir.exists() and shader_dir.is_dir():
                shader_dirs.append(shader_dir)
    
    return shader_dirs


def install_resources():
    """Main installation function."""
    print("=" * 60)
    print("RoboCute Resource Installation")
    print("=" * 60)
    
    # Find target directory
    target_dir, build_system = find_target_dir()
    if not target_dir:
        print("ERROR: Could not find build target directory.")
        print("  Please ensure you have built the project first.")
        print("  Expected locations:")
        print("    - CMake: build_cmake/bin/Release (or Debug/RelWithDebInfo)")
        print(f"    - xmake: build/{PLATFORM}/{ARCH}/release (or debug)")
        sys.exit(1)
    
    print(f"Using build system: {build_system}")
    print(f"Target directory: {target_dir}")
    print()
    
    # 1. Extract test_scene archive
    test_scene_archive = rel(f"build/download/{TEST_SCENE_ARCHIVE}")
    if not test_scene_archive.exists():
        print(f"WARNING: Test scene archive not found: {test_scene_archive}")
        print("  Skipping test_scene installation.")
    else:
        print(f"Installing test_scene from {test_scene_archive.name}...")
        extract_7z(test_scene_archive, target_dir)
        print()
    
    # 2. Extract render_resources archive
    render_resources_archive = rel(f"build/download/{RENDER_RESOURCE_NAME}")
    if not render_resources_archive.exists():
        print(f"WARNING: Render resources archive not found: {render_resources_archive}")
        print("  Skipping render_resources installation.")
    else:
        print(f"Installing render_resources from {render_resources_archive.name}...")
        extract_7z(render_resources_archive, target_dir)
        print()
    
    # 3. Copy shader build results
    shader_dirs = find_shader_build_dirs(build_system, target_dir)
    if not shader_dirs:
        print("WARNING: No shader build directories found.")
        print("  Expected locations:")
        print("    - CMake: build_cmake/bin/shader_build_dx, shader_build_vk")
        print(f"    - xmake: build/{PLATFORM}/{ARCH}/shader_build_dx, shader_build_vk")
    else:
        # Determine destination (parent of target_dir)
        if build_system == "cmake":
            # For cmake: target_dir is build_cmake/bin/Release
            # Destination should be build_cmake/bin
            dest_dir = target_dir.parent
        else:
            # For xmake: target_dir is build/{platform}/{arch}/{mode}
            # Destination should be build/{platform}/{arch}
            dest_dir = target_dir.parent
        
        print(f"Copying shader build results to {dest_dir}...")
        # Track which shader names we've already copied to avoid duplicates
        copied_shader_names = set()
        
        for shader_dir in shader_dirs:
            # Verify source directory still exists before copying
            if not shader_dir.exists():
                print(f"  WARNING: {shader_dir} no longer exists, skipping...")
                continue
            
            if not shader_dir.is_dir():
                print(f"  WARNING: {shader_dir} is not a directory, skipping...")
                continue
            
            dest_shader_dir = dest_dir / shader_dir.name
            
            # Skip if we've already copied this shader name (avoid duplicate copies)
            if shader_dir.name in copied_shader_names:
                # Check if source and destination are the same (already in place)
                if shader_dir.resolve() == dest_shader_dir.resolve():
                    print(f"  Skipping {shader_dir.name} (already in destination location)")
                else:
                    print(f"  Skipping {shader_dir.name} (already copied from another location)")
                continue
            
            # If source and destination are the same, skip copying
            if shader_dir.resolve() == dest_shader_dir.resolve():
                print(f"  Skipping {shader_dir.name} (already in correct location)")
                copied_shader_names.add(shader_dir.name)
                continue
            
            if dest_shader_dir.exists():
                print(f"  Removing existing {dest_shader_dir.name}...")
                shutil.rmtree(dest_shader_dir)
            
            print(f"  Copying {shader_dir.name} from {shader_dir}...")
            try:
                shutil.copytree(shader_dir, dest_shader_dir)
                print(f"  ✓ Copied {shader_dir.name} to {dest_dir}")
                copied_shader_names.add(shader_dir.name)
            except FileNotFoundError as e:
                print(f"  ERROR: Failed to copy {shader_dir.name}: {e}")
                print(f"    Source: {shader_dir}")
                print(f"    Destination: {dest_shader_dir}")
            except Exception as e:
                print(f"  ERROR: Unexpected error copying {shader_dir.name}: {e}")
        print()
    
    print("=" * 60)
    print("Installation completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    install_resources()

