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
    cbox_path = str("cornell_box.obj")
    sky_path = str("sky.exr")
    world_path = str(runtime_dir.parent / "world")

    ctx = RBCContext()
    ctx.init_world(world_path, world_path)
    ctx.init_device(backend_name, program_path, shader_path)
    ctx.init_render()
    project = Project()
    project.init(str(runtime_dir.parent / "project"))
    # do we need scan?
    # project.scan_project().wait()
    cbox_mesh_request = project.import_mesh(cbox_path, "")
    skybox_tex_request = project.import_texture(sky_path, "")
    skybox_tex = TextureResource(skybox_tex_request.get_result_release())
    del skybox_tex_request  # handle released, async-request already useless
    skybox_tex.set_skybox()
    del skybox_tex
    ctx.create_window("py_window", uint2(1920, 1080), True)

    # ctx.load_skybox(sky_path, uint2(4096, 2048))

    # make_submesh
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = triangle_count // 2
    ##################### Load cbox
    cbox_mesh = MeshResource(cbox_mesh_request.get_result_release(), True)
    ##################### Create a test cube
    # cube_mesh = MeshResource()
    # cube_mesh.create_empty(
    #     submesh_offsets,
    #     vertex_count,
    #     triangle_count,
    #     0, False, False
    # )
    # mesh_array = np.ndarray(
    #     vertex_count * 4 + triangle_count * 3,
    #     dtype=np.float32,
    #     buffer=cube_mesh.data_buffer()
    # )
    # create_mesh_array(mesh_array)
    # cube_mesh.install()

    # mat_default_json = json.loads("{}")
    # mat_default_json["base_albedo"] = [1, 0.6, 0.7]
    # mat = MaterialResource()
    # mat.load_from_json(json.dumps(mat_default_json))
    # mat.install()

    # mat_default_json["base_albedo"] = [0.6, 1, 0.7]
    # second_mat = MaterialResource()
    # second_mat.load_from_json(json.dumps(mat_default_json))
    # second_mat.install()

    # mat_vector = capsule_vector()
    # mat_vector.emplace_back(mat._handle)
    # mat_vector.emplace_back(second_mat._handle)
    # mat_vector.emplace_back(mat._handle)
    # mat_vector.emplace_back(second_mat._handle)
    # mat_vector.emplace_back(mat._handle)
    # mat_vector.emplace_back(second_mat._handle)
    # mat_vector.emplace_back(mat._handle)
    # mat_vector.emplace_back(second_mat._handle)

    mat0 = '{"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]}'
    mat1 = '{"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]}'
    mat2 = '{"type": "pbr", "specular_roughness": 0.5, "weight_metallic": 1.0, "base_albedo": [0.630, 0.065, 0.050]}'
    light_mat_desc = (
        '{"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]}'
    )
    basic_mat = MaterialResource()
    basic_mat.load_from_json(mat0)

    left_wall_mat = MaterialResource()
    left_wall_mat.load_from_json(mat1)

    right_wall_mat = MaterialResource()
    right_wall_mat.load_from_json(mat2)

    light_mat = MaterialResource()
    light_mat.load_from_json(light_mat_desc)

    mat_vector = capsule_vector()
    mat_vector.emplace_back(basic_mat._handle)
    mat_vector.emplace_back(basic_mat._handle)
    mat_vector.emplace_back(basic_mat._handle)
    mat_vector.emplace_back(left_wall_mat._handle)
    mat_vector.emplace_back(right_wall_mat._handle)
    mat_vector.emplace_back(basic_mat._handle)
    mat_vector.emplace_back(basic_mat._handle)
    mat_vector.emplace_back(light_mat._handle)

    entity = Entity()
    trans = TransformComponent(entity.add_component("TransformComponent"))
    render = RenderComponent(entity.add_component("RenderComponent"))
    trans.set_pos(double3(0, -1, 3), False)
    trans.set_rotation(float4(0, -1, 0, 0), False)
    render.update_object(mat_vector, cbox_mesh)
    del mat_vector

    light_entity = Entity()
    light_trans = TransformComponent(light_entity.add_component("TransformComponent"))
    light = LightComponent(light_entity.add_component("LightComponent"))
    light_trans.set_trs_matrix(
        make_double4x4(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0.5, 4, 1), False
    )

    light.add_area_light(float3(30, 20, 20), True)
    while not ctx.should_close():
        ctx.tick()


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
