import os
import time
from pathlib import Path
from rbc_ext.rbc_backend import *
import numpy as np
import json
import math


def main():
    backend_name = 'vk'
    runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
    program_path = str(runtime_dir.parent / 'debug')
    shader_path = str(runtime_dir.parent / f"shader_build_{backend_name}")
    sky_path = str(runtime_dir.parent / 'sky.bytes')

    ctx = RBCContext()
    ctx.init_device(backend_name, program_path, shader_path)
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
    # Texture:
    # tex_data = np.ones(1024 * 1024 * 4, dtype=np.float32)
    # tex = ctx.create_texture(
    #     tex_data,
    #     LCPixelStorage.FLOAT4,
    #     uint2(1024, 1024),
    #     1
    # )
    # mat_default_json['base_albedo_tex'] = ctx.texture_heap_idx(tex)

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
            1, 1.5, 4, 1
        ),
        float3(0, 0, 1.0) * 100,
        True
    )

    obj_changed = False
    start_time = None
    mesh_array = np.ndarray(vertex_count * 4 + triangle_count * 3, dtype=np.float32, buffer=ctx.get_mesh_data(mesh))
    while not ctx.should_close():
        end_time = time.perf_counter()
        if start_time:
            for i in range(4, 8):
                mesh_array[i * 4 + 1] = 0.5 + math.sin(end_time - start_time) * 0.2
        ctx.update_mesh(mesh, False)
        if not obj_changed:
            if start_time and end_time - start_time > 2:
                obj_changed = True
                mat_default_json['base_albedo'] = [1, 0.84, 0]
                mat_default_json['emission_luminance'] = [0, 0, 0]
                mat_default_json['specular_roughness'] = 0.0
                mat_default_json['weight_metallic'] = 1.0
                mat_vector.clear()
                mat_vector.emplace_back(second_mat)
                mat_vector.emplace_back(mat)
                ctx.update_pbr_material(second_mat, json.dumps(mat_default_json))
                ctx.update_object(obj,
                                  make_float4x4(
                                      1, 0, 0, 0,
                                      0, 1, 0, 0,
                                      0, 0, 1, 0,
                                      -0.8, -1.0, 4, 1
                                  ), mesh, mat_vector)
                ctx.reset_frame_index()
        ctx.tick()
        if not start_time:
            start_time = time.perf_counter()

    destroy_object(mesh)
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
    # make_submesh
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = last_tri_size

    return mesh_array, vertex_count, triangle_count, submesh_offsets


if __name__ == '__main__':
    main()
