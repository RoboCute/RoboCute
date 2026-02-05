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

EXPORT = False

def main():
    if len(sys.argv) < 2:
        print("must input scene root-dir")
        exit(1)
    backend_name = "vk"
    runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
    program_path = str(runtime_dir.parent / "debug")
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
    ctx.init_display("py_window", resolution, True, True)
    print("importing")
    scene = project.import_scene("test_scene.scene", "")
    print("installing")
    scene.install()
    print("installed")
    last_time = time.time()
    frame_index = 0
    image_index = 0
    tick_stage = TickStage.PathTracingPreview
    entity = make_mesh()
    
    @luisa.func
    def write_buffer_vec3_to_img(buffer, element_offset, img):
        set_block_size(16, 8, 1)
        id = dispatch_id().xy
        idx = id.x + id.y * dispatch_size().x
        idx *= 3
        idx += element_offset
        value = float3(
            buffer.read(idx),
            buffer.read(idx + 1),
            buffer.read(idx + 2)
        )
        img.write(id, float4(value, 1.0))

    @luisa.func
    def write_buffer_vec1_to_img(buffer, scale, element_offset, img):
        set_block_size(16, 8, 1)
        id = dispatch_id().xy
        idx = id.x + id.y * dispatch_size().x
        idx += element_offset
        idx = id.x + id.y * dispatch_size().x
        value = float4(buffer.read(idx) * scale)
        img.write(id, value)

    display_cam = ctx.create_display_cam()
    transform = TransformComponent(
        display_cam.entity().get_component("TransformComponent"))
    display_cam.enable_camera()

    transform.set_pos(double3(0, 0, -1), False)
    if EXPORT:
        geometry_buffer = Buffer(
            resolution.x * resolution.y * (1 + 3 + 3 + 3), float)
        display_cam.set_geometry_export_buffer(
            geometry_buffer.info(),
            RendererGeometryType(int(RendererGeometryType.Depth) | int(RendererGeometryType.Normal) | int(
                RendererGeometryType.Emission) | int(RendererGeometryType.Albedo))
        )
    else:
        ctx.enable_camera_control()
    while not ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        display_cam.set_frame_index(frame_index)
        if ctx.tick(
            delta_time, tick_stage, True
        ):
            frame_index = 0
        else:
            frame_index += 1
        if frame_index == 64:
            if entity is not None:
                print('deleting entity')
                entity.dispose()
                entity = None
                frame_index = 0
        if EXPORT and frame_index == 128:
            # frame_index = 0
            ctx.denoise()
            # Make frame purple
            # make_img_purple(img, dispatch_size=(img.width, img.height, 1))
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/frame_{image_index}.png")
            )
            img = ctx.display_image()
            element_offset = 0
            write_buffer_vec1_to_img(
                geometry_buffer, 0.2, element_offset, img, dispatch_size=(img.width, img.height, 1))
            pixel_size = img.width * img.height
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/depth_{image_index}.png")
            )
            element_offset += pixel_size

            write_buffer_vec3_to_img(
                geometry_buffer, element_offset, img, dispatch_size=(img.width, img.height, 1))
            pixel_size = img.width * img.height
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/normal_{image_index}.png")
            )
            element_offset += pixel_size * 3

            write_buffer_vec3_to_img(
                geometry_buffer, element_offset, img, dispatch_size=(img.width, img.height, 1))
            pixel_size = img.width * img.height
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/emission_{image_index}.png")
            )
            element_offset += pixel_size * 3

            write_buffer_vec3_to_img(
                geometry_buffer, element_offset, img, dispatch_size=(img.width, img.height, 1))
            pixel_size = img.width * img.height
            ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/albedo_{image_index}.png")
            )
            element_offset += pixel_size * 3

            tick_stage = TickStage.NONE
            image_index += 1
            display_cam.clear_geometry_export_buffer()
            geometry_buffer.dispose()

vertex_count = 16
triangle_count = 24

def make_mesh():
    mat0 = MaterialResource()
    mat0.load_from_json('{"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]}')
    mat1 = MaterialResource()
    mat1.load_from_json('{"type": "pbr", "specular_roughness": 0.5, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]}')
    mat_vector = capsule_vector()
    mat_vector.emplace_back(mat0._handle)
    mat_vector.emplace_back(mat1._handle)
    entity = Entity()
    trans = TransformComponent(entity.add_component("TransformComponent"))
    render = RenderComponent(entity.add_component("RenderComponent"))
    trans.set_pos(double3(0, -1, 1), False)
    trans.set_rotation(float4(0, -1, 0, 0), False)
    cube_mesh = MeshResource()
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = triangle_count // 2
    
    cube_mesh.create_empty(
        submesh_offsets,
        vertex_count,
        triangle_count,
        0, False, False
    )
    mesh_array = np.ndarray(
        vertex_count * 4 + triangle_count * 3,
        dtype=np.float32,
        buffer=cube_mesh.data_buffer()
    )
    create_mesh_array(mesh_array)
    cube_mesh.install()
    render.update_object(mat_vector, cube_mesh)
    return entity
    
    
def create_mesh_array(mesh_array):

    # create a cube
    if mesh_array.size != vertex_count * 4 + triangle_count * 3:
        raise Exception("Bad mesh-array size")
    vertex_arr = np.ndarray(
        vertex_count * 4, dtype=np.float32, buffer=mesh_array.data)
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
