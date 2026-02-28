import os
import time
from pathlib import Path
import numpy as np
import math
import argparse
from typing import Optional

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
from robocute.rbc_ext._C import lcapi_c as lcapi
import samples.cli as cli
import mat_builtin as mat

vertex_count = 16
"""网格顶点总数(两个立方体, 每个8个顶点)"""

triangle_count = 24
"""网格三角形总数(两个立方体, 每个12个三角形)"""

app: rbc.app.App = None


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
    mat.openpbr_set_specular_roughness(mat0_json, 0.8)
    mat.openpbr_set_weight_metallic(mat0_json, 0.3)
    mat.openpbr_set_base_albedo(mat0_json, (0.8, 0.8, 0.8))
    mat.openpbr_set_base_albedo_tex(mat0_json, tex)

    mat0.load_from_json(mat.openpbr_dump_to_json(mat0_json))
    del mat0_json

    mat1 = re.world.MaterialResource()

    mat1_json = mat.OpenPBRInterface(app._project)
    mat.openpbr_set_specular_roughness(mat1_json, 0.5)
    mat.openpbr_set_weight_metallic(mat1_json, 0.3)
    mat.openpbr_set_base_albedo(mat1_json, (0.140, 0.450, 0.091))
    mat.openpbr_set_base_albedo_tex(mat1_json, tex)

    mat1.load_from_json(mat.openpbr_dump_to_json(mat1_json))
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
    cube_mesh = re.world.MeshResource()
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = triangle_count // 2

    cube_mesh.create_empty(
        submesh_offsets, vertex_count, triangle_count, 1, False, False
    )
    # Data layout: positions (vertex_count * 4 floats) + UVs (vertex_count * 2 floats)
    # + indices (triangle_count * 3 uint32s)
    mesh_array = np.ndarray(
        vertex_count * 4 + vertex_count * 2 + triangle_count * 3,
        dtype=np.float32,
        buffer=cube_mesh.data_buffer(),
    )
    create_mesh_array(mesh_array)
    cube_mesh.install()
    render.update_object(mat_vector, cube_mesh)
    return entity


def create_mesh_array(mesh_array):
    """
    生成两个立方体的顶点数据和索引数据

    数据布局:
        - 位置数据: 每个顶点4个float(x, y, z, w), 共16个顶点
        - UV数据: 每个顶点2个float(u, v), 共16个顶点
        - 索引数据: 每个三角形3个uint32索引, 共24个三角形

    顶点缓冲区格式:
        [position_data (vertex_count * 4 floats)]
        [uv_data (vertex_count * 2 floats)]
        [index_data (triangle_count * 3 uint32s)]

    Args:
        mesh_array: numpy 数组, 用于存储生成的网格数据

    Raises:
        Exception: 当 mesh_array 大小不匹配预期时抛出

    立方体顶点索引定义:
        0: 左下后 (-0.5, -0.5, -0.5)
        1: 左下前 (-0.5, -0.5, 0.5)
        2: 右下后 (0.5, -0.5, -0.5)
        3: 右下前 (0.5, -0.5, 0.5)
        4: 左上后 (-0.5, 0.5, -0.5)
        5: 左上前 (-0.5, 0.5, 0.5)
        6: 右上后 (0.5, 0.5, -0.5)
        7: 右上前 (0.5, 0.5, 0.5)
    """

    # create a cube
    expected_size = vertex_count * 4 + vertex_count * 2 + triangle_count * 3
    if mesh_array.size != expected_size:
        raise Exception(
            f"Bad mesh-array size: {mesh_array.size} != {expected_size}")

    # Position data: vertex_count * 4 floats
    vertex_arr = np.ndarray(
        vertex_count * 4, dtype=np.float32, buffer=mesh_array.data)

    # UV data: vertex_count * 2 floats, after position data
    uv_arr = np.ndarray(
        vertex_count * 2,
        dtype=np.float32,
        buffer=mesh_array.data,
        offset=vertex_arr.size * vertex_arr.itemsize,
    )

    # Index data: after UV data
    indices_arr = np.ndarray(
        shape=triangle_count * 3,
        dtype=np.uint32,
        buffer=mesh_array.data,
        offset=vertex_arr.size * vertex_arr.itemsize + uv_arr.size * uv_arr.itemsize,
    )
    vert_size = 0
    uv_size = 0
    index_size = 0
    offset = lc.float4(0)
    scale = lc.float4(1)

    def push_vec4(x, y, z):
        """向顶点缓冲区添加一个 float4 顶点

        Args:
            x, y, z: 顶点坐标分量
        """
        nonlocal vert_size, offset, scale
        vec = lc.float4(x, y, z, 0) * scale + offset
        for i in range(4):
            vertex_arr[vert_size + i] = vec[i]
        vert_size += 4

    def push_indices(idx: int):
        """向索引缓冲区添加一个顶点索引

        Args:
            idx: 顶点索引值
        """
        nonlocal index_size
        indices_arr[index_size] = idx
        index_size += 1

    def push_vert():
        """向顶点缓冲区添加8个立方体顶点(应用当前 offset 和 scale 变换)"""
        push_vec4(-0.5, -0.5, -0.5)  # 0: 左下后
        push_vec4(-0.5, -0.5, 0.5)  # 1: 左下前
        push_vec4(0.5, -0.5, -0.5)  # 2: 右下后
        push_vec4(0.5, -0.5, 0.5)  # 3: 右下前
        push_vec4(-0.5, 0.5, -0.5)  # 4: 左上后
        push_vec4(-0.5, 0.5, 0.5)  # 5: 左上前
        push_vec4(0.5, 0.5, -0.5)  # 6: 右上后
        push_vec4(0.5, 0.5, 0.5)  # 7: 右上前

    def push_uvs():
        """向UV缓冲区添加8个立方体顶点的UV坐标

        UV映射基于立方体展开,为每个顶点分配适当的UV坐标:
            底面顶点(0-3): y=0, v=0
            顶面顶点(4-7): y=1, v=1
            前后左右根据x/z坐标分配u坐标
        """
        nonlocal uv_size
        # UV coordinates for 8 vertices of a cube
        # Mapping based on vertex positions for consistent texture mapping
        uv_coords = [
            (0.0, 0.0),  # 0: 左下后 (-0.5, -0.5, -0.5)
            (0.0, 1.0),  # 1: 左下前 (-0.5, -0.5, 0.5)
            (1.0, 0.0),  # 2: 右下后 (0.5, -0.5, -0.5)
            (1.0, 1.0),  # 3: 右下前 (0.5, -0.5, 0.5)
            (0.0, 0.0),  # 4: 左上后 (-0.5, 0.5, -0.5)
            (0.0, 1.0),  # 5: 左上前 (-0.5, 0.5, 0.5)
            (1.0, 0.0),  # 6: 右上后 (0.5, 0.5, -0.5)
            (1.0, 1.0),  # 7: 右上前 (0.5, 0.5, 0.5)
        ]
        for u, v in uv_coords:
            uv_arr[uv_size] = u
            uv_arr[uv_size + 1] = v
            uv_size += 2

    # First cube: positions and UVs
    push_vert()
    last_vert_size = vert_size
    push_uvs()

    # Second cube: positions and UVs
    offset = lc.float4(0, 1, 0, 0)
    scale = lc.float4(0.4, 0.4, 0.4, 0)
    push_vert()
    push_uvs()

    # Triangle indices
    def push_cube_triangles():
        """向索引缓冲区添加一个立方体的12个三角形(6个面, 每个面2个三角形)"""
        # 底面 (0, 1, 2) 和 (1, 3, 2)
        push_indices(0)
        push_indices(1)
        push_indices(2)
        push_indices(1)
        push_indices(3)
        push_indices(2)
        # 顶面 (4, 5, 6) 和 (5, 7, 6)
        push_indices(4)
        push_indices(5)
        push_indices(6)
        push_indices(5)
        push_indices(7)
        push_indices(6)
        # 左面 (0, 1, 4) 和 (1, 5, 4)
        push_indices(0)
        push_indices(1)
        push_indices(4)
        push_indices(1)
        push_indices(5)
        push_indices(4)
        # 右面 (2, 3, 6) 和 (3, 7, 6)
        push_indices(2)
        push_indices(3)
        push_indices(6)
        push_indices(3)
        push_indices(7)
        push_indices(6)
        # 后面 (0, 2, 4) 和 (2, 6, 4)
        push_indices(0)
        push_indices(2)
        push_indices(4)
        push_indices(2)
        push_indices(6)
        push_indices(4)
        # 前面 (1, 3, 5) 和 (3, 7, 5)
        push_indices(1)
        push_indices(3)
        push_indices(5)
        push_indices(3)
        push_indices(7)
        push_indices(5)

    push_cube_triangles()
    last_index_size = index_size
    # index size to triangle size
    last_tri_size = last_index_size // 3
    push_cube_triangles()
    for i in range(last_index_size, index_size):
        indices_arr[i] += last_vert_size // 4


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

    # ###################
    # @lc.func
    # def write_buffer_vec3_to_img(buffer, element_offset, img):
    #     """
    #     Luisa kernel: 将 float3 数据从缓冲区写入图像

    #     用于将几何缓冲区中的 vec3 数据 (如 normal albedo) 可视化为图像

    #     Args:
    #         buffer: 源数据缓冲区
    #         element_offset: 在缓冲区中的起始偏移量
    #         img: 目标图像
    #     """
    #     lset_block_size(16, 8, 1)
    #     id = dispatch_id().xy
    #     idx = id.x + id.y * dispatch_size().x
    #     idx *= 3
    #     idx += element_offset
    #     value = lc.float3(buffer.read(idx), buffer.read(idx + 1), buffer.read(idx + 2))
    #     img.write(id, lc.float4(value.x, value.y, value.z, 1.0))

    # @lc.func
    # def write_buffer_vec1_to_img(buffer, scale, element_offset, img):
    #     """
    #     Luisa kernel: 将 float 数据从缓冲区写入图像

    #     用于将几何缓冲区中的标量数据(如深度)可视化为灰度图像

    #     Args:
    #         buffer: 源数据缓冲区
    #         scale: 缩放因子, 用于调整数据范围到可视范围
    #         element_offset: 在缓冲区中的起始偏移量
    #         img: 目标图像
    #     """
    #     lc.set_block_size(16, 8, 1)
    #     id = dispatch_id().xy
    #     idx = id.x + id.y * dispatch_size().x
    #     idx += element_offset
    #     idx = id.x + id.y * dispatch_size().x
    #     value = lc.float4(buffer.read(idx) * scale)
    #     img.write(id, value)

    global app
    app = rbc.app.App()  # rbc app singleton
    app.init(project_path=project_path, backend_name=args.backend)
    tex = app._project.import_texture('test_grid.png', 4, True)
    print(tex.size())
    if not app.ctx:
        print("Context not Valid!")
        return

    resolution = lc.uint2(1920, 1080)
    app.init_display(resolution.x, resolution.y)
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
    tui_exec = tui_table.execute_cli(
        cli.executor.async_input,
        lambda c: c == 'exit'
    )

    # clear_shader = lc.Shader('gui/clear_shader.bin')

    # if EXPORT:
    #     geometry_buffer = lc.Buffer(
    #         resolution.x * resolution.y * (1 + 3 + 3 + 3), float
    #     )
    #     app.display_cam.set_geometry_export_buffer(
    #         geometry_buffer.info(),
    #         re.world.RendererGeometryType(
    #             int(re.world.RendererGeometryType.Depth)
    #             | int(re.world.RendererGeometryType.Normal)
    #             | int(re.world.RendererGeometryType.Emission)
    #             | int(re.world.RendererGeometryType.Albedo)
    #         ),
    #     )
    # else:
    app.ctx.enable_camera_control()

    if not app.scene:
        print("Scene not Valid!")
        return

    entity = make_cube_mesh(app.scene, tex=tex)
    last_time = time.time()

    tick_stage = re.world.TickStage.PathTracingPreview
    # app.run()
    while not app.ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        app.display_cam.set_frame_index(frame_index)
        if app.ctx.tick(delta_time, tick_stage, True) or app._requires_reset:
            frame_index = 0
            app._requires_reset = False
        else:
            frame_index += 1
        # EDITING example

        # app.ctx.editing_add_click_requires("my_click", lc.float2(0.5))
        # render_comp = app.ctx.editing_query_click_requires("my_click")
        # if render_comp:
        #     print(re.world.TransformComponent(render_comp.entity().get_component('TransformComponent')).position())

        if EXPORT and frame_index == 128:
            img = app.display_image()
            print(img.width)
            print(img.height)
        #     # frame_index = 0
            app.ctx.denoise()
            app.ctx.save_display_image_to(
                str(Path(__file__).parent /
                    f"screenshot/frame_{image_index}.png")
            )
            tick_stage = re.world.TickStage.NONE
            # clear_shader(img, lc.float4(1, 0, 1, 1), dispatch_size=(img.width, img.height, 1))
        #     img = app.ctx.display_image()
        #     element_offset = 0
        #     write_buffer_vec1_to_img(
        #         geometry_buffer,
        #         0.2,
        #         element_offset,
        #         img,
        #         dispatch_size=(img.width, img.height, 1),
        #     )
        #     pixel_size = img.width * img.height
        #     app.ctx.save_display_image_to(
        #         str(Path(__file__).parent / f"screenshot/depth_{image_index}.png")
        #     )
        #     element_offset += pixel_size

        #     write_buffer_vec3_to_img(
        #         geometry_buffer,
        #         element_offset,
        #         img,
        #         dispatch_size=(img.width, img.height, 1),
        #     )
        #     pixel_size = img.width * img.height
        #     app.ctx.save_display_image_to(
        #         str(Path(__file__).parent / f"screenshot/normal_{image_index}.png")
        #     )
        #     element_offset += pixel_size * 3

        #     write_buffer_vec3_to_img(
        #         geometry_buffer,
        #         element_offset,
        #         img,
        #         dispatch_size=(img.width, img.height, 1),
        #     )
        #     pixel_size = img.width * img.height
        #     app.ctx.save_display_image_to(
        #         str(Path(__file__).parent / f"screenshot/emission_{image_index}.png")
        #     )
        #     element_offset += pixel_size * 3

        #     write_buffer_vec3_to_img(
        #         geometry_buffer,
        #         element_offset,
        #         img,
        #         dispatch_size=(img.width, img.height, 1),
        #     )
        #     pixel_size = img.width * img.height
        #     app.ctx.save_display_image_to(
        #         str(Path(__file__).parent / f"screenshot/albedo_{image_index}.png")
        #     )
        #     element_offset += pixel_size * 3

        #     image_index += 1
        #     app.display_cam.clear_geometry_export_buffer()
        try:
            value = next(tui_exec)
            if value is not None:
                print(value)
        except StopIteration:
            print('Exit from TUI!')
            break


if __name__ == "__main__":
    main()
