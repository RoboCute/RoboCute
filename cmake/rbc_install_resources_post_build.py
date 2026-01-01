"""
CMake post-build script for installing RoboCute resources.
This script is called by CMake after build to extract archives and copy shader files.
"""
import os
import sys
import shutil
import subprocess
from pathlib import Path

def find_7z_executable():
    """Find 7z executable for extracting archives."""
    possible_paths = [
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
    ]
    
    # Try to find in PATH
    for path in possible_paths:
        if os.path.exists(path):
            return path
    
    # Try shutil.which
    seven_zip = shutil.which("7z") or shutil.which("7za")
    if seven_zip:
        return seven_zip
    
    return None

def extract_7z(archive_path: Path, extract_to: Path):
    """Extract a 7z archive to the specified directory."""
    seven_zip = find_7z_executable()
    if not seven_zip:
        print(f"WARNING: 7z executable not found. Cannot extract {archive_path.name}")
        return False
    
    extract_to.mkdir(parents=True, exist_ok=True)
    
    print(f"Extracting {archive_path.name} to {extract_to}...")
    try:
        subprocess.check_call(
            [seven_zip, "x", str(archive_path), f"-o{extract_to}", "-y"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE
        )
        print(f"✓ Successfully extracted {archive_path.name}")
        return True
    except subprocess.CalledProcessError:
        print(f"ERROR: Failed to extract {archive_path.name}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Usage: rbc_install_resources_post_build.py <project_root> <output_dir>")
        print("WARNING: Insufficient arguments provided - skipping resource installation")
        # Don't exit with error code - allow build to continue
        sys.exit(0)
    
    project_root = Path(sys.argv[1]).resolve()
    output_dir = Path(sys.argv[2]).resolve()
    
    print("=" * 60)
    print("RoboCute Resource Installation (CMake Post-Build)")
    print("=" * 60)
    print(f"Project root: {project_root}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Test scene version (should match install.py)
    TEST_SCENE_VERSION = "v1.0.1"
    TEST_SCENE_ARCHIVE = project_root / f"build/download/test_scene_{TEST_SCENE_VERSION}.7z"
    
    # Render resources archive name (should match prepare.py)
    RENDER_RESOURCE_NAME = "render_resources-v1.0.0.7z"
    RENDER_RESOURCES_ARCHIVE = project_root / f"build/download/{RENDER_RESOURCE_NAME}"
    
    # 1. Extract test_scene archive
    if TEST_SCENE_ARCHIVE.exists():
        extract_7z(TEST_SCENE_ARCHIVE, output_dir)
    else:
        print(f"WARNING: Test scene archive not found: {TEST_SCENE_ARCHIVE}")
        print("  Skipping test_scene installation.")
    print()
    
    # 2. Extract render_resources archive
    if RENDER_RESOURCES_ARCHIVE.exists():
        extract_7z(RENDER_RESOURCES_ARCHIVE, output_dir)
    else:
        print(f"WARNING: Render resources archive not found: {RENDER_RESOURCES_ARCHIVE}")
        print("  Skipping render_resources installation.")
    print()
    
    # 3. Copy shader build results
    # Shader directories are typically in the build directory
    # Find shader directories in common locations
    # For Qt Creator: output_dir might be build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug/bin/Debug
    # For CMake: output_dir might be build_cmake/bin/Release or build/bin/Debug
    
    # Determine build directory and shader destination
    config_names = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
    
    # Check if output_dir ends with a config name (multi-config generator)
    if output_dir.name in config_names:
        # Multi-config: output_dir is build/.../bin/Debug
        # Shader dest should be build/.../bin (parent of output_dir)
        dest_dir = output_dir.parent
        build_dir = dest_dir.parent
    elif output_dir.parent.name == "bin":
        # Single-config or bin is parent: output_dir is build/.../bin
        # Shader dest should be build/... (parent of bin)
        dest_dir = output_dir.parent
        build_dir = dest_dir
    else:
        # Fallback: assume output_dir is the build directory
        dest_dir = output_dir.parent
        build_dir = output_dir.parent
    
    shader_dirs = []
    possible_shader_locations = [
        build_dir / "shader_build_dx",
        build_dir / "shader_build_vk",
        output_dir.parent / "shader_build_dx",
        output_dir.parent / "shader_build_vk",
        output_dir / "shader_build_dx",
        output_dir / "shader_build_vk",
    ]
    
    # Remove duplicates while preserving order
    seen = set()
    unique_locations = []
    for loc in possible_shader_locations:
        loc_str = str(loc.resolve()) if loc.exists() else str(loc)
        if loc_str not in seen:
            seen.add(loc_str)
            unique_locations.append(loc)
    
    for shader_path in unique_locations:
        if shader_path.exists() and shader_path.is_dir():
            shader_dirs.append(shader_path)
    
    if not shader_dirs:
        print("WARNING: No shader build directories found.")
        print("  Expected locations:")
        print(f"    - {build_dir}/shader_build_dx, shader_build_vk")
        print(f"    - {output_dir.parent}/shader_build_dx, shader_build_vk")
    else:
        
        print(f"Copying shader build results to {dest_dir}...")
        copied_shader_names = set()
        
        for shader_dir in shader_dirs:
            if shader_dir.name in copied_shader_names:
                continue
            
            dest_shader_dir = dest_dir / shader_dir.name
            
            # Skip if source and destination are the same
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
            except Exception as e:
                print(f"  ERROR: Failed to copy {shader_dir.name}: {e}")
        print()
    
    print("=" * 60)
    print("Resource installation completed!")
    print("=" * 60)
    
    # Always exit with success to prevent build failure
    # Warnings are printed but don't stop the build
    sys.exit(0)

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"ERROR: Unexpected error during resource installation: {e}")
        import traceback
        traceback.print_exc()
        # Always exit with success to prevent build failure
        sys.exit(0)

