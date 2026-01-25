import os
import sys
import time
from pathlib import Path
from rbc_ext.generated.rbc_backend import *
from rbc_ext.generated.world import *
import numpy as np
import json
import math

# Auto-setup RBC_RUNTIME_DIR if not set
if "RBC_RUNTIME_DIR" not in os.environ:
    project_root = Path(__file__).parent.parent.parent
    # Try to find the build directory
    found = False
    runtime_dir = project_root / "build" / "windows" / "x64" / "release"
    if runtime_dir.exists():
        os.environ["RBC_RUNTIME_DIR"] = str(runtime_dir)
        # Also add to PATH for DLL loading
        os.environ["PATH"] = f"{runtime_dir};{os.environ.get('PATH', '')}"
        print(f"Auto-detected RBC_RUNTIME_DIR: {runtime_dir}")
        found = True
    if not found:
        raise RuntimeError(
            f"Could not auto-detect RBC_RUNTIME_DIR. "
            f"Searched in: {project_root / 'build' / 'windows' / 'x64' / 'release'}"
        )

EXPORT = False


def main():
    if len(sys.argv) < 2:
        print("must input scene root-dir")
        exit(1)
    backend_name = "vk"
    runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
    program_path = str(runtime_dir.parent / "release")
    shader_path = str(runtime_dir.parent / f"shader_build_{backend_name}")
    world_path = str(Path(sys.argv[1]) / "library")

    ctx = RBCContext()
    ctx.init_world(world_path, world_path)
    ctx.init_device(backend_name, program_path, shader_path)
    ctx.init_render()
    project = Project()
    project.init(str(Path(sys.argv[1]) / "assets"))
    print("scaning")
    project.scan_project()
    print("scanned")
    resolution = uint2(1920, 1080)
    ctx.create_window("py_window", resolution, True)
    print("importing")
    scene = project.import_scene("test_scene.scene", "")
    print("installing")
    scene.install()
    print("installed")
    last_time = time.time()
    frame_index = 0
    image_index = 0
    while not ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        ctx.tick(
            delta_time, resolution, frame_index, TickStage.PathTracingPreview, True
        )
        frame_index += 1
        if EXPORT and frame_index == 64:
            frame_index = 0
            ctx.denoise()
            ctx.save_display_image_to(
                str(Path(__file__).parent / f"screenshot/frame_{image_index}.png")
            )
            image_index += 1
    del scene


if __name__ == "__main__":
    main()
