import os
import time
from pathlib import Path
from rbc_ext.generated.rbc_backend import (
    RBCContext,
    uint2,
    capsule_vector,
    float4,
    float3,
    make_float4x4,
    make_double4x4,
    destroy_object,
)
from rbc_ext.generated.world import *
import numpy as np
import json
import math

# Auto-setup RBC_RUNTIME_DIR if not set
if "RBC_RUNTIME_DIR" not in os.environ:
    project_root = Path(__file__).parent.parent.parent
    # Try to find the build directory
    found = False
    runtime_dir = project_root / "build" / "windows" / "x64" / "debug"
    if runtime_dir.exists():
        os.environ["RBC_RUNTIME_DIR"] = str(runtime_dir)
        # Also add to PATH for DLL loading
        os.environ["PATH"] = f"{runtime_dir};{os.environ.get('PATH', '')}"
        print(f"Auto-detected RBC_RUNTIME_DIR: {runtime_dir}")
        found = True
    if not found:
        raise RuntimeError(
            f"Could not auto-detect RBC_RUNTIME_DIR. "
            f"Searched in: {project_root / 'build' / 'windows' / 'x64' / 'debug'}"
        )


def main():
    backend_name = "vk"
    runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
    program_path = str(runtime_dir.parent / "debug")
    shader_path = str(runtime_dir.parent / f"shader_build_{backend_name}")
    world_path = str(runtime_dir.parent / 'world')

    ctx = RBCContext()
    ctx.init_world(world_path, world_path)
    ctx.init_device(backend_name, program_path, shader_path)
    ctx.init_render()
    project = Project()
    project.init("C:/dev/RoboCute/build/download/test_scene_v1.0.1")
    print('scaning')
    project.scan_project().wait()
    print('scanned')
    ctx.create_window("py_window", uint2(1920, 1080), True)
    print('importing')
    scene_request = project.import_scene('test_scene.scene', '')
    print('loading')
    scene = Scene(scene_request.get_result_release())
    print('installing')
    scene.install()
    print('installed')
    while not ctx.should_close():
        ctx.tick()


if __name__ == "__main__":
    main()
