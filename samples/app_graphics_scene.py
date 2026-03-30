import mat_builtin as mat
import samples.cli as cli
from robocute.rbc_ext._C import lcapi_c as lcapi
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
import robocute as rbc
import os
import sys
import time
from pathlib import Path
import numpy as np
import math
import argparse
from typing import Optional
from PIL import Image
from samples.mesh_builder import MeshBuilder

# Add parent directory to path for samples module imports
script_dir = Path(__file__).parent
if str(script_dir.parent) not in sys.path:
    sys.path.insert(0, str(script_dir.parent))


app: rbc.app.App = None


def int_array_to_rgb(int_array: np.ndarray) -> np.ndarray:
    """Convert an array of integer IDs to RGB image array using PCG hash.

    Args:
        int_array: 2D array of integer IDs with shape (height, width)

    Returns:
        3D RGB array with shape (height, width, 3) and dtype uint8
    """
    height, width = int_array.shape
    rgb_array = np.zeros((height, width, 3), dtype=np.uint8)

    # PCG constants from pcg.hpp
    PRIME32_2 = np.uint32(2246822519)
    PRIME32_3 = np.uint32(3266489917)
    PRIME32_4 = np.uint32(668265263)
    PRIME32_5 = np.uint32(374761393)

    # Flatten for vectorized processing
    flat_ids = int_array.flatten().astype(np.uint32)

    # PCGSampler(uint v) constructor - initialize state from seed
    h32 = flat_ids + PRIME32_5
    h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)))
    h32 = PRIME32_2 * (h32 ^ (h32 >> 15))
    h32 = PRIME32_3 * (h32 ^ (h32 >> 13))
    state = h32 ^ (h32 >> 16)

    # Generate 3 random values for RGB channels using PCG nextui()
    for i in range(3):
        # nextui(): generate next random uint
        old_state = state.copy()
        state = state * np.uint32(747796405) + np.uint32(2891336453)
        word = ((old_state >> ((old_state >> 28) + 4))
                ^ old_state) * np.uint32(277803737)
        rand_val = (word >> 22) ^ word

        # Convert to float in [0, 1) then to uint8 in [0, 255]
        # Using division by 2^32 for uniform distribution
        rgb_array[:, :, i] = (rand_val / np.float32(4294967296.0) * 255.0).astype(
            np.uint8
        ).reshape(height, width)

    return rgb_array


def make_cube_mesh(scene: re.world.Scene, tex: re.world.TextureResource):
    """
    创建一个包含两个立方体的动态网格实体

    创建的实体包含:
        - TransformComponent: 控制实体位置和旋转
        - RenderComponent: 包含网格和材质渲染信息
        - MeshResource: 包含两个子网格的立方体数据
        - 两种 PBR 材质: 白色粗糙材质(mat0)和绿色金属材质(mat1)

    网格结构:
        - 第一个立方体位于原点, 缩放为1.0
        - 第二个立方体位于 (0, 1, 0), 缩放为0.4

    Returns:
        Entity: 创建的实体对象, 包含完整的渲染组件
    """
    mat0 = re.world.MaterialResource()

    mat0_json = mat.OpenPBRInterface(app._project)
    mat0_json.set_specular_roughness(0.8)
    mat0_json.set_weight_metallic(0.3)
    mat0_json.set_base_albedo((0.8, 0.8, 0.8))
    mat0_json.set_base_albedo_tex(tex)

    mat0.load_from_json(mat0_json.dump_to_json())
    del mat0_json

    mat1 = re.world.MaterialResource()

    mat1_json = mat.OpenPBRInterface(app._project)
    mat1_json.set_specular_roughness(0.5)
    mat1_json.set_weight_metallic(0.3)
    mat1_json.set_base_albedo((0.140, 0.450, 0.091))
    mat1_json.set_base_albedo_tex(tex)

    mat1.load_from_json(mat1_json.dump_to_json())
    del mat1_json

    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(mat0._handle)
    mat_vector.emplace_back(mat1._handle)
    entity = scene.add_entity()
    # Test entity by name
    entity.set_name("test_cube")
    entity = scene.get_entity_by_name("test_cube")
    assert entity._handle is not None
    trans = re.world.TransformComponent(
        entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))

    trans.set_pos(lc.double3(0, -1, 1), False)
    trans.set_rotation(lc.float4(0, -1, 0, 0), False)

    # Use MeshBuilder to create the mesh
    builder = MeshBuilder()

    # Add UV set
    builder.add_uv_set()

    # First cube: offset=(0,0,0), scale=1.0
    _add_cube_to_builder(builder, (0, 0, 0), 1.0)

    # Second cube: offset=(0,1,0), scale=0.4
    _add_cube_to_builder(builder, (0, 1, 0), 0.4)

    # Create mesh resource and write data
    cube_mesh = builder.write_to_mesh()
    cube_mesh.install()
    render.update_object(mat_vector, cube_mesh)
    return entity


def _add_cube_to_builder(builder: MeshBuilder, offset: tuple[float, float, float], scale: float):
    """Add a cube to the mesh builder with given offset and scale.
    
    Args:
        builder: MeshBuilder instance to add cube to
        offset: Position offset for the cube (x, y, z)
        scale: Scale factor for the cube
    """
    start_vertex = builder.vertex_count()
    s = scale
    ox, oy, oz = offset

    # 8 vertices of a cube
    positions = [
        (-0.5 * s + ox, -0.5 * s + oy, -0.5 * s + oz),  # 0: left-bottom-back
        (-0.5 * s + ox, -0.5 * s + oy, 0.5 * s + oz),   # 1: left-bottom-front
        (0.5 * s + ox, -0.5 * s + oy, -0.5 * s + oz),   # 2: right-bottom-back
        (0.5 * s + ox, -0.5 * s + oy, 0.5 * s + oz),    # 3: right-bottom-front
        (-0.5 * s + ox, 0.5 * s + oy, -0.5 * s + oz),   # 4: left-top-back
        (-0.5 * s + ox, 0.5 * s + oy, 0.5 * s + oz),    # 5: left-top-front
        (0.5 * s + ox, 0.5 * s + oy, -0.5 * s + oz),    # 6: right-top-back
        (0.5 * s + ox, 0.5 * s + oy, 0.5 * s + oz),     # 7: right-top-front
    ]
    for pos in positions:
        builder.add_vertex(pos)

    # UV coordinates for 8 vertices
    uvs = [
        (0.0, 0.0),  # 0
        (0.0, 1.0),  # 1
        (1.0, 0.0),  # 2
        (1.0, 1.0),  # 3
        (0.0, 0.0),  # 4
        (0.0, 1.0),  # 5
        (1.0, 0.0),  # 6
        (1.0, 1.0),  # 7
    ]
    for uv in uvs:
        builder.uvs[0] = np.vstack([builder.uvs[0], np.array(uv, dtype=np.float32).reshape(1, 2)])

    # Add submesh for this cube
    submesh_idx = builder.add_submesh()

    # 12 triangles (6 faces, 2 triangles each)
    triangles = [
        # Bottom face
        (0, 1, 2), (1, 3, 2),
        # Top face
        (4, 5, 6), (5, 7, 6),
        # Left face
        (0, 1, 4), (1, 5, 4),
        # Right face
        (2, 3, 6), (3, 7, 6),
        # Back face
        (0, 2, 4), (2, 6, 4),
        # Front face
        (1, 3, 5), (3, 7, 5),
    ]

    for a, b, c in triangles:
        builder.add_triangle(submesh_idx, start_vertex + a, start_vertex + b, start_vertex + c)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        help="rbc project path, the directory containing rbc_project.json",
        required=True,
    )
    parser.add_argument("-o", "--output", action="store_true", help="Export")
    args = parser.parse_args()

    project_path = Path(args.project)
    EXPORT = args.output
    global app
    app = rbc.app.App()  # rbc app singleton
    app.init(project_path=project_path, backend_name=args.backend)
    tex = app._project.import_texture('test_grid.png', 1, False)
    print(tex.size())
    if not app.ctx:
        print("Context not Valid!")
        return

    resolution = lc.uint2(1920, 1080)
    app.init_display(resolution.x, resolution.y) # create_window=False  to use headless
    if not app.display_cam:
        print("Display not Valid!")
        return

    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 0, -1), False)
    # TUI test
    tui_table = cli.executor.CLITable()
    frame_index = 0
    image_index = 0
    cli.rbc_app.app = app
    cli.builtin.rbc_app_register(tui_table)
    # print(tui_table.dump_func_table())

    tui_exec = None

    # clear_shader = lc.Shader('gui/clear_shader.bin')

    if EXPORT:
        channel_size = (1 + 3 + 1 + 1 + 2 + 3 + 3 + 1)
        geometry_buffer = lc.Buffer(
            resolution.x * resolution.y * channel_size, float
        )
        app.display_cam.set_geometry_export_buffer(
            geometry_buffer.info(),
            re.world.RendererGeometryType(
                int(re.world.RendererGeometryType.Depth)
                | int(re.world.RendererGeometryType.Normal)
                | int(re.world.RendererGeometryType.ObjectID)
                | int(re.world.RendererGeometryType.PrimID)
                | int(re.world.RendererGeometryType.Barycentric)
                | int(re.world.RendererGeometryType.Emission)
                | int(re.world.RendererGeometryType.Albedo)
                | int(re.world.RendererGeometryType.MaterialID)
            ),
        )
    if app._window_created:
        app.ctx.enable_camera_control()

    if not app.scene:
        print("Scene not Valid!")
        return

    # DO THIS: change texture in shader
    # move_shader = lc.Shader('geometry/move_mesh.bin')
    # move_tex = lc.Shader('geometry/move_color.bin')
    # my_tex = lc.Image2D.import_native(float, tex.device_texture())
    # move_tex(
    #     my_tex,
    #     lc.float4(0, 1, 1, 1),
    #     dispatch_size=(my_tex.width, my_tex.height, 1)
    # )
    entity = make_cube_mesh(app.scene, tex=tex)
    last_time = time.time()

    def tick_logic():  # run every frame
        nonlocal last_time

        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        # build after update

        # DO THIS: change mesh in shader
        # render = re.world.RenderComponent(entity.get_component("RenderComponent"))
        # buffer = lc.Buffer.import_native(lc.float3, render.mesh().device_data_buffer())
        # move_shader(
        #     buffer,
        #     delta_time,
        #     dispatch_size=(vertex_count, 1, 1)
        # )
        # render.mesh().build_before_tick()
        nonlocal EXPORT, tui_exec, frame_index, geometry_buffer
        frame_index += 1
        if EXPORT and frame_index == 128:
            EXPORT = False
            img = app.display_image()
            app.ctx.denoise()
            app.ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/frame_{image_index}.png")
            )
            expected_size = resolution.x * resolution.y * channel_size
            geometry_array = np.empty(shape=expected_size, dtype=np.float32)
            # Assert geometry_array's size same as geometry_buffer's size
            assert geometry_buffer.size == expected_size, (
                f"geometry_array size mismatch: {geometry_buffer.size} != {expected_size}"
            )
            geometry_buffer.copy_to(geometry_array)
            print(geometry_buffer.size)
            offset = 0
            pixel_size = resolution.x * resolution.y
            # float 1-channel buffer
            depth_array = geometry_array[offset:offset + pixel_size]
            offset += pixel_size
            # float 3-channel buffer
            normal_array = geometry_array[offset:offset + pixel_size * 3]
            offset += pixel_size * 3
            object_id_array = geometry_array[offset:offset +
                                             pixel_size].view(dtype=np.uint32)
            offset += pixel_size
            prim_id_array = geometry_array[offset:offset +
                                           pixel_size].view(dtype=np.uint32)
            offset += pixel_size
            bary_array = geometry_array[offset:offset+pixel_size * 2]
            offset += pixel_size * 2
            # float 3-channel buffer
            emission_array = geometry_array[offset:offset + pixel_size * 3]
            offset += pixel_size * 3
            # float 3-channel buffer
            albedo_array = geometry_array[offset:offset + pixel_size * 3]
            offset += pixel_size * 3
            # int 1-channel buffer
            material_id_array = geometry_array[offset:offset +
                                               pixel_size].view(dtype=np.uint32)
            offset += pixel_size
            
            # Save depth, normal, emission, albedo as PNG images
            screenshot_dir = Path(__file__).parent / "screenshot"
            screenshot_dir.mkdir(exist_ok=True)

            # Reshape arrays to image dimensions
            height, width = resolution.y, resolution.x

            # Depth: normalize to 0-255 for visualization
            depth_img = depth_array.reshape(height, width)
            depth_min, depth_max = 0.01, 10.0
            if depth_max > depth_min:
                depth_norm = (depth_img - depth_min) / \
                    (depth_max - depth_min) * 255
            else:
                depth_norm = np.zeros_like(depth_img)
            depth_pil = Image.fromarray(depth_norm.astype(np.uint8), mode='L')
            depth_pil.save(screenshot_dir / f"depth_{image_index}.png")

            # Normal: reshape and convert to 0-255 range
            normal_img = normal_array.reshape(height, width, 3)
            normal_norm = np.clip((normal_img + 1.0) *
                                  127.5, 0, 255).astype(np.uint8)
            normal_pil = Image.fromarray(normal_norm, mode='RGB')
            normal_pil.save(screenshot_dir / f"normal_{image_index}.png")

            # Emission: reshape and convert to 0-255 range
            emission_img = emission_array.reshape(height, width, 3)
            emission_norm = np.clip(
                emission_img * 255, 0, 255).astype(np.uint8)
            emission_pil = Image.fromarray(emission_norm, mode='RGB')
            emission_pil.save(screenshot_dir / f"emission_{image_index}.png")

            # Albedo: reshape and convert to 0-255 range
            albedo_img = albedo_array.reshape(height, width, 3)
            albedo_norm = np.clip(albedo_img * 255, 0, 255).astype(np.uint8)
            albedo_pil = Image.fromarray(albedo_norm, mode='RGB')
            albedo_pil.save(screenshot_dir / f"albedo_{image_index}.png")
            # Export object_id and prim_id as RGB PNG images
            # Using a simple hash-based color generation for integer IDs

            # Reshape object_id and prim_id to 2D image dimensions
            object_id_img = object_id_array.reshape(height, width)
            prim_id_img = prim_id_array.reshape(height, width)

            # Convert to RGB using vectorized operation
            object_id_rgb = int_array_to_rgb(object_id_img)
            prim_id_rgb = int_array_to_rgb(prim_id_img)

            # Save as PNG images
            object_id_pil = Image.fromarray(object_id_rgb, mode='RGB')
            object_id_pil.save(screenshot_dir / f"object_id_{image_index}.png")

            prim_id_pil = Image.fromarray(prim_id_rgb, mode='RGB')
            prim_id_pil.save(screenshot_dir / f"prim_id_{image_index}.png")

            # Material ID: reshape and convert to RGB
            material_id_img = material_id_array.reshape(height, width)
            material_id_rgb = int_array_to_rgb(material_id_img)
            material_id_pil = Image.fromarray(material_id_rgb, mode='RGB')
            material_id_pil.save(screenshot_dir / f"material_id_{image_index}.png")

            # Barycentric: reshape and convert to 0-255 range (2 channels: RGB with B=0)
            bary_img = bary_array.reshape(height, width, 2)
            bary_norm = np.clip(bary_img * 255, 0, 255).astype(np.uint8)
            # Convert to 3-channel RGB
            bary_rgb = np.zeros((height, width, 3), dtype=np.uint8)
            bary_rgb[:, :, 0] = bary_norm[:, :, 0]  # R channel
            bary_rgb[:, :, 1] = bary_norm[:, :, 1]  # G channel
            bary_pil = Image.fromarray(bary_rgb, mode='RGB')
            bary_pil.save(screenshot_dir / f"bary_{image_index}.png")
            app.display_cam.clear_geometry_export_buffer()
            del geometry_buffer
            geometry_buffer = None
            print('Channel saved.')
            exit(0)
        if tui_exec is None:
            tui_exec = tui_table.execute_cli(
                cli.executor.async_input,
                lambda c: c == 'exit'
            )
        try:
            value = next(tui_exec)
            if value is not None:
                print(value)
        except StopIteration:
            print('Exit from TUI!')
            app.call_exit()  # End the loop
    app.set_user_callback(tick_logic)
    # app.set_ground_plane_mode('yes')
    # Enable AO mode
    render_settings = app.display_cam.render_settings()
    
    # render_settings.set_offline_spp(4)
    # render_settings.set_offline_origin_bounce(1)
    # render_settings.set_offline_indirect_bounce(0)
    # render_settings.set_enable_ao_mode(True)
    # render_settings.set_ao_max_radius(lc.float4(1.5, 1.0, 0.5, 0.2))
    # render_settings.set_offline_origin_bounce(1)
    # render_settings.set_offline_indirect_bounce(0)

    app.run(prepare_denoise=EXPORT)


if __name__ == "__main__":
    main()
