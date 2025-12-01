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
    mesh = ctx.create_mesh(
        mesh_array,
        vertex_count,
        False,
        False,
        0,
        triangle_count
    )
    mat = ctx.create_pbr_material(create_orange_mat())
    mat_json = ctx.get_material_data(mat)
    print(mat_json)
    
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
    
    ##################### load and store mesh through file
    # f = open('test_mesh.bytes', 'wb')
    # f.write(mesh_array)
    # f.close()
    # mesh = ctx.load_mesh(
    #     'test_mesh.bytes',
    #     0,
    #     vertex_count,
    #     False,
    #     False,
    #     0,
    #     triangle_count
    # )
    mesh_buffer = ctx.get_mesh_data(mesh)
    # TODO: how to transform void* to buffer?
    vertex_array = np.ndarray(shape=8 * 4, dtype=mesh_array.dtype, buffer=mesh_buffer, offset=0)
    triangle_array = np.ndarray(shape=6*6, dtype=np.uint32, buffer=mesh_buffer, offset=vertex_array.itemsize * vertex_array.size)
    print(f'readback mesh size {vertex_array.size}')
    for i in range(8):
        print(float3(vertex_array[i * 4], vertex_array[i * 4 + 1], vertex_array[i * 4 + 2]))
    for i in range(12):
        print(int3(triangle_array[i * 3], triangle_array[i * 3 + 1], triangle_array[i * 3 + 2]))
    
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
            1,1,4,1
        ),
        float3(1, 0.8, 0.7) * 100,
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

def create_orange_mat():
    return '''{
    "weight_base": 1.0,
    "weight_diffuse_roughness": 0.0,
    "weight_specular": 1.0,
    "weight_metallic": 0.0,
    "weight_metallic_roughness_tex": 4294967295,
    "weight_subsurface": 0.0,
    "weight_transmission": 0.0,
    "weight_thin_film": 0.0,
    "weight_fuzz": 0.0,
    "weight_coat": 0.0,
    "weight_diffraction": 0.0,
    "geometry_cutout_threshold": 0.30000001192092896,
    "geometry_opacity": 1.0,
    "geometry_opacity_tex": 4294967295,
    "geometry_thickness": 0.5,
    "geometry_thin_walled": true,
    "geometry_nested_priority": 0,
    "geometry_bump_scale": 1.0,
    "geometry_normal_tex": 4294967295,
    "uv_scale": [
        1.0,
        1.0
    ],
    "uv_offset": [
        0.0,
        0.0
    ],
    "specular_color": [
        1.0,
        1.0,
        1.0
    ],
    "specular_roughness": 0.30000001192092896,
    "specular_roughness_anisotropy": 0.0,
    "specular_anisotropy_level_tex": 4294967295,
    "specular_roughness_anisotropy_angle": 0.0,
    "specular_anisotropy_angle_tex": 4294967295,
    "specular_ior": 1.5,
    "emission_luminance": [
        0.0,
        0.0,
        0.0
    ],
    "emission_tex": 4294967295,
    "base_albedo": [
        0.9,
        0.7,
        0.3
    ],
    "base_albedo_tex": 4294967295,
    "subsurface_color": [
        0.800000011920929,
        0.800000011920929,
        0.800000011920929
    ],
    "subsurface_radius": 0.05000000074505806,
    "subsurface_radius_scale": [
        1.0,
        0.5,
        0.25
    ],
    "subsurface_scatter_anisotropy": 0.0,
    "transmission_color": [
        1.0,
        1.0,
        1.0
    ],
    "transmission_depth": 0.0,
    "transmission_scatter": [
        0.0,
        0.0,
        0.0
    ],
    "transmission_scatter_anisotropy": 0.0,
    "transmission_dispersion_scale": 0.0,
    "transmission_dispersion_abbe_number": 20.0,
    "coat_color": [
        1.0,
        1.0,
        1.0
    ],
    "coat_roughness": 0.0,
    "coat_roughness_anisotropy": 0.0,
    "coat_roughness_anisotropy_angle": 0.0,
    "coat_ior": 1.600000023841858,
    "coat_darkening": 1.0,
    "coat_roughening": 1.0,
    "fuzz_color": [
        1.0,
        1.0,
        1.0
    ],
    "fuzz_roughness": 0.5,
    "diffraction_color": [
        1.0,
        1.0,
        1.0
    ],
    "diffraction_thickness": 0.5,
    "diffraction_inv_pitch_x": 0.3333333432674408,
    "diffraction_inv_pitch_y": 0.0,
    "diffraction_angle": 0.0,
    "diffraction_lobe_count": 5,
    "diffraction_type": 1,
    "thin_film_thickness": 0.5,
    "thin_film_ior": 1.399999976158142
}'''

if __name__ == '__main__':
    main()