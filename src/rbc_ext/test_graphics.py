import os
import time
from pathlib import Path
from rbc_ext.rbc_backend import *
import numpy as np
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

    mesh_array, vertex_count, triangle_count = create_mesh_array()
    
    # tex_array = np.zeros(1024 * 1024 * 4, dtype=np.float32)
    # tex = ctx.create_texture(
    #     tex_array,
    #     LCPixelStorage.FLOAT4,
    #     uint2(1024, 1024),
    #     SamplerAddress.EDGE,
    #     SamplerFilter.LINEAR_POINT,
    #     1,
    #     False
    # )
    mesh = ctx.create_mesh(
        mesh_array,
        vertex_count,
        False,
        False,
        0,
        triangle_count
    )
    obj = ctx.create_object(
        make_float4x4(
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            -0.8,-0.8,4,1
        ),
        mesh
    )
    area_light = ctx.add_area_light(
        make_float4x4(
            1,0,0,0,
            0,0,1,0,
            0,-1,0,0,
            1,1,3,1
        ),
        float3(1, 0.8, 0.7) * 20,
        True
    )
    while not ctx.should_close():
        ctx.tick()

    ctx.remove_object(obj) 
    ctx.remove_mesh(mesh)
    del ctx


def create_mesh_array():
    # create a cube
    vertex_count = 8
    mesh_array = np.empty(vertex_count * 4 + 6 * 6, dtype=np.float32)
    vertex_arr = np.ndarray(
        vertex_count * 4, dtype=np.float32, buffer=mesh_array.data)
    indices_arr = np.ndarray(shape=6 * 6, dtype=np.uint32,
                            buffer=mesh_array.data, offset=vertex_arr.size * vertex_arr.itemsize)
    size = 0
    def push_vec4(x,y,z):
        nonlocal size
        vec = float4(x,y,z,0)
        for i in range(4):
            vertex_arr[size + i] = vec[i]
        size += 4
    def push_indices(idx:int):
        nonlocal size
        indices_arr[size]=idx
        size += 1

    push_vec4(-0.5, -0.5, -0.5)  # 0: Left Bottom Back
    push_vec4(-0.5, -0.5, 0.5)  # 1: Left Bottom Front
    push_vec4(0.5, -0.5, -0.5)  # 2: Right Buttom Back
    push_vec4(0.5, -0.5, 0.5)  # 3: Right Buttom Front
    push_vec4(-0.5, 0.5, -0.5)  # 4: Left Up Back
    push_vec4(-0.5, 0.5, 0.5)  # 5: Left Up Front
    push_vec4(0.5, 0.5, -0.5)  # 6: Right Up Back
    push_vec4(0.5, 0.5, 0.5)   # 7: Right Up Front
    size = 0
    # Buttom face
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
    return mesh_array, vertex_count, indices_arr.size // 3

if __name__ == '__main__':
    main()