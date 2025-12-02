import os
import time
from pathlib import Path
from rbc_ext.rbc_backend import *
import numpy as np
import json


def main():
    backend_name = 'vk'
    runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
    program_path = str(runtime_dir.parent / 'debug')
    shader_path = str(runtime_dir.parent / f"shader_build_{backend_name}")
    sky_path = str(runtime_dir.parent / 'sky.bytes')

    ctx = RBCContext()
    ctx.init_device('vk', program_path, shader_path)
    ctx.init_render()
    ctx.load_skybox(sky_path, uint2(4096, 2048))
    ctx.create_window("py_window", uint2(1920, 1080), True)

    mesh_array, vertex_count, triangle_count, submesh_offset = create_mesh_array()
    mesh = ctx.create_mesh(
        mesh_array,
        vertex_count,
        False,
        False,
        0,
        triangle_count,
        submesh_offset
    )
    mat = ctx.create_pbr_material("{}")
    mat_default_json = json.loads(ctx.get_material_json(mat))
    mat_default_json['base_albedo'] = [0, 0, 0]
    mat_default_json['emission_luminance'] = [100, 0, 0]
    second_mat = ctx.create_pbr_material(json.dumps(mat_default_json))

    mat_vector = capsule_vector()
    mat_vector.emplace_back(mat)
    mat_vector.emplace_back(second_mat)

    obj = ctx.create_object(
        make_float4x4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -0.8, -0.8, 4, 1
        ),
        mesh,
        mat_vector
    )
    area_light = ctx.add_area_light(
        make_float4x4(
            1, 0, 0, 0,
            0, 0, 1, 0,
            0, -1, 0, 0,
            1, 1, 4, 1
        ),
        float3(0, 0, 1.0) * 100,
        True
    )
    start_time = time.perf_counter()
    while not ctx.should_close():
        # if obj:
        #     end_time = time.perf_counter()
        #     if end_time - start_time > 2:
        #         ctx.reset_frame_index()
        #         ctx.remove_object(obj)
        #         obj = None
        ctx.tick()

    ctx.remove_mesh(mesh)
    del ctx


def create_mesh_array():
    # create a cube
    vertex_count = 16
    triangle_count = 24
    mesh_array = np.empty(
        vertex_count * 4 + triangle_count * 3, dtype=np.float32)
    vertex_arr = np.ndarray(
        vertex_count * 4, dtype=np.float32, buffer=mesh_array.data)
    indices_arr = np.ndarray(shape=triangle_count * 3, dtype=np.uint32,
                             buffer=mesh_array.data, offset=vertex_arr.size * vertex_arr.itemsize)
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
        push_vec4(0.5, 0.5, 0.5)   # 7: Right Up Front
    push_vert()
    last_vert_size = size
    offset = float4(0, 1, 0, 0)
    scale = float4(0.2, 0.2, 0.2, 0)
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
    # make_submesh
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = last_tri_size

    return mesh_array, vertex_count, triangle_count, submesh_offsets


if __name__ == '__main__':
    main()
