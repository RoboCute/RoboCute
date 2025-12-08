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
    destroy_object,
)
import numpy as np
import json
import math

vertex_count = 16
triangle_count = 24

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
    sky_path = str(runtime_dir.parent / "sky.bytes")
    ctx = RBCContext()
    ctx.init_device(backend_name, program_path, shader_path)
    ctx.init_render()
    ctx.load_skybox(sky_path, uint2(4096, 2048))
    ctx.create_window("py_window", uint2(1920, 1080), True)

    # make_submesh
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = triangle_count // 2
    mesh = ctx.create_mesh(
        vertex_count, False, False, 0, triangle_count, submesh_offsets
    )
    mesh_array = np.ndarray(
        vertex_count * 4 + triangle_count * 3,
        dtype=np.float32,
        buffer=ctx.get_mesh_data(mesh),
    )
    create_mesh_array(mesh_array)
    ctx.update_mesh(mesh, False)
    mat = ctx.create_pbr_material()
    ctx.update_material(mat, "{}")  # use default value

    mat_default_json = json.loads(ctx.get_material_json(mat))
    mat_default_json["base_albedo"] = [0, 0, 0]
    mat_default_json["emission_luminance"] = [100, 0, 0]

    # Use texture to turn cube to red
    # TEX_SIZE = 1024
    # tex = ctx.create_texture(
    #     LCPixelStorage.FLOAT4,
    #     uint2(TEX_SIZE, TEX_SIZE),
    #     1
    # )
    # mat_default_json['base_albedo_tex'] = ctx.texture_heap_idx(tex)
    # tex_array = np.ndarray(TEX_SIZE * TEX_SIZE * 4, dtype=np.float32, buffer = ctx.get_texture_data(tex))
    # for x in range(TEX_SIZE):
    #     for y in range(TEX_SIZE):
    #         idx = (y * TEX_SIZE + x) * 4
    #         tex_array[idx] = 1.0
    #         tex_array[idx + 1] = 0.
    #         tex_array[idx + 2] = 0.
    # ctx.update_texture(tex)

    second_mat = ctx.create_pbr_material()
    ctx.update_material(second_mat, json.dumps(mat_default_json))

    mat_vector = capsule_vector()
    mat_vector.emplace_back(mat)
    mat_vector.emplace_back(second_mat)

    obj = ctx.create_object(
        make_float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.8, -0.8, 4, 1),
        mesh,
        mat_vector,
    )
    area_light = ctx.add_area_light(
        make_float4x4(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 1, 1.5, 4, 1),
        float3(0, 0, 1.0) * 100,
        True,
    )

    obj_changed = False
    start_time = None
    while not ctx.should_close():
        end_time = time.perf_counter()
        if start_time:
            for i in range(4, 8):
                mesh_array[i * 4 + 1] = 0.5 + math.sin(end_time - start_time) * 0.2
        ctx.update_mesh(mesh, True)
        if not obj_changed:
            if start_time and end_time - start_time > 2:
                obj_changed = True
                mat_default_json["base_albedo"] = [1, 0.84, 0]
                mat_default_json["emission_luminance"] = [0, 0, 0]
                mat_default_json["specular_roughness"] = 0.0
                mat_default_json["weight_metallic"] = 1.0
                mat_vector.clear()
                mat_vector.emplace_back(second_mat)
                mat_vector.emplace_back(mat)
                ctx.update_material(second_mat, json.dumps(mat_default_json))
                ctx.update_object(
                    obj,
                    make_float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.8, -1.0, 4, 1),
                    mesh,
                    mat_vector,
                )
                ctx.reset_frame_index()
        ctx.tick()
        if not start_time:
            start_time = time.perf_counter()

    destroy_object(mesh)
    del ctx


def create_mesh_array(mesh_array):
    # create a cube
    if mesh_array.size != vertex_count * 4 + triangle_count * 3:
        raise Exception("Bad mesh-array size")
    vertex_arr = np.ndarray(vertex_count * 4, dtype=np.float32, buffer=mesh_array.data)
    indices_arr = np.ndarray(
        shape=triangle_count * 3,
        dtype=np.uint32,
        buffer=mesh_array.data,
        offset=vertex_arr.size * vertex_arr.itemsize,
    )
    size = 0
    offset = float4(0)
    scale = float4(1)

    def push_vec4(x, y, z):
        nonlocal size, offset, scale
        vec = float4(x, y, z, 0) * scale + offset
        for i in range(4):
            vertex_arr[size + i] = vec[i]
        size += 4

    def push_indices(idx: int):
        nonlocal size
        indices_arr[size] = idx
        size += 1

    def push_vert():
        push_vec4(-0.5, -0.5, -0.5)  # 0: Left Bottom Back
        push_vec4(-0.5, -0.5, 0.5)  # 1: Left Bottom Front
        push_vec4(0.5, -0.5, -0.5)  # 2: Right Buttom Back
        push_vec4(0.5, -0.5, 0.5)  # 3: Right Buttom Front
        push_vec4(-0.5, 0.5, -0.5)  # 4: Left Up Back
        push_vec4(-0.5, 0.5, 0.5)  # 5: Left Up Front
        push_vec4(0.5, 0.5, -0.5)  # 6: Right Up Back
        push_vec4(0.5, 0.5, 0.5)  # 7: Right Up Front

    push_vert()
    last_vert_size = size
    offset = float4(0, 1, 0, 0)
    scale = float4(0.4, 0.4, 0.4, 0)
    push_vert()
    size = 0
    # Buttom face

    def push_cube_triangles():
        push_indices(0)
        push_indices(1)
        push_indices(2)
        push_indices(1)
        push_indices(3)
        push_indices(2)
        # Up face
        push_indices(4)
        push_indices(5)
        push_indices(6)
        push_indices(5)
        push_indices(7)
        push_indices(6)
        # Left face
        push_indices(0)
        push_indices(1)
        push_indices(4)
        push_indices(1)
        push_indices(5)
        push_indices(4)
        # Right face
        push_indices(2)
        push_indices(3)
        push_indices(6)
        push_indices(3)
        push_indices(7)
        push_indices(6)
        # Back face
        push_indices(0)
        push_indices(2)
        push_indices(4)
        push_indices(2)
        push_indices(6)
        push_indices(4)
        # Front face
        push_indices(1)
        push_indices(3)
        push_indices(5)
        push_indices(3)
        push_indices(7)
        push_indices(5)

    push_cube_triangles()
    last_index_size = size
    # index size to triangle size
    last_tri_size = last_index_size // 3
    push_cube_triangles()
    for i in range(last_index_size, size):
        indices_arr[i] += last_vert_size // 4


if __name__ == "__main__":
    main()
