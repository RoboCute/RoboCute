import os
import sys
import time
from pathlib import Path
import rbc_ext.luisa as luisa
from rbc_ext.generated.world import *
import numpy as np
import json
import math
from rbc_ext.luisa import *

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
    luisa.init()
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
    tick_stage = TickStage.PathTracingPreview
    @luisa.func
    def make_img_purple(img):
        set_block_size(16, 8, 1)
        img.write(dispatch_id().xy, img.read(
            dispatch_id().xy) * float4(1, 0, 1, 1))
    display_cam = ctx.create_display_cam()
    transform = TransformComponent(display_cam.entity().get_component("TransformComponent"))
    transform.set_pos(double3(0, 0, -2), False)
    display_cam.enable_camera()
    while not ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        display_cam.set_frame_data(frame_index, delta_time)
        ctx.tick(
            tick_stage, True
        )
        frame_index += 1
        if EXPORT and frame_index == 128:
            # frame_index = 0
            ctx.denoise()
            img = ctx.display_image()
            ###### Make frame purple
            make_img_purple(img, dispatch_size=(img.width, img.height, 1))
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/frame_{image_index}.png")
            )
            tick_stage = TickStage.NONE
            image_index += 1
    del scene
    del display_cam
    del ctx


if __name__ == "__main__":
    main()
