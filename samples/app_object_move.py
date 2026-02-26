import os
import time
from pathlib import Path
import numpy as np
import math
import argparse

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re


vertex_count = 16
"""网格顶点总数(两个立方体, 每个8个顶点)"""

triangle_count = 24
"""网格三角形总数(两个立方体, 每个12个三角形)"""


def make_cube_mesh(scene: re.world.Scene):
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
    
    mat0_json = re.world.OpenPBRInterface()
    mat0_json.set_specular_roughness(0.8)
    mat0_json.set_weight_metallic(0.3)
    mat0_json.set_base_albedo(re.world.float3(1.0, 0.710, 0.680))
    
    mat0 = re.world.MaterialResource()
    mat0.load_from_json(mat0_json.dump_to_json())
    del mat0_json
    
    mat1 = re.world.MaterialResource()
    mat1_json = re.world.OpenPBRInterface()
    mat1_json.set_specular_roughness(0.5)
    mat1_json.set_weight_metallic(0.3)
    mat1_json.set_base_albedo(re.world.float3(0.140, 0.450, 0.091))
    
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
    trans = re.world.TransformComponent(entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))

    trans.set_pos(lc.double3(0, -1, 1), False)
    trans.set_rotation(lc.float4(0, -1, 0, 0), False)

    # bind mesh resource
    cube_mesh = re.world.MeshResource()
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    # first submesh start at 0
    submesh_offsets[0] = 0
    # first submesh start at 'last_tri_size'

    submesh_offsets[1] = triangle_count // 2

    cube_mesh.create_empty(
        submesh_offsets, vertex_count, triangle_count, 0, False, False
    )
    mesh_array = np.ndarray(
        vertex_count * 4 + triangle_count * 3,
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
        - 顶点数据: 每个顶点4个float(x, y, z, w), 共16个顶点
        - 索引数据: 每个三角形3个uint32索引, 共24个三角形

    顶点缓冲区格式 (vertex_count * 4 floats):
        [cube1_vert0_x, cube1_vert0_y, cube1_vert0_z, 0, ...]

    索引缓冲区格式 (triangle_count * 3 uint32s):
        位于顶点数据之后, 每个三角形3个顶点索引

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
    offset = lc.float4(0)
    scale = lc.float4(1)

    def push_vec4(x, y, z):
        """向顶点缓冲区添加一个 float4 顶点

        Args:
            x, y, z: 顶点坐标分量
        """
        nonlocal size, offset, scale
        vec = lc.float4(x, y, z, 0) * scale + offset
        for i in range(4):
            vertex_arr[size + i] = vec[i]
        size += 4

    def push_indices(idx: int):
        """向索引缓冲区添加一个顶点索引

        Args:
            idx: 顶点索引值
        """
        nonlocal size
        indices_arr[size] = idx
        size += 1

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

    push_vert()
    last_vert_size = size
    offset = lc.float4(0, 1, 0, 0)
    scale = lc.float4(0.4, 0.4, 0.4, 0)
    push_vert()
    size = 0
    # Buttom face

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
    last_index_size = size
    # index size to triangle size
    last_tri_size = last_index_size // 3
    push_cube_triangles()
    for i in range(last_index_size, size):
        indices_arr[i] += last_vert_size // 4


def test_callback(ptr):
    app = rbc.app.App()  # singleton
    if not app.ctx:
        # Callback called when context is invalid
        return
    comp = re.world.DataComponent(ptr)
    entity = comp.entity()
    transform = re.world.TransformComponent(entity.get_component("TransformComponent"))
    cur_time = time.time()
    # 移动动画参数
    move_speed = 2.0  # 移动速度
    move_range = 1.0  # 移动范围 (上下各 1 单位)
    base_y = -1.0  # Y 轴基准位置
    # 计算新的 Y 位置 (正弦波周期性移动)
    # 使用当前时间计算位置,实现平滑的周期性上下移动
    new_y = base_y + math.sin(cur_time * move_speed) * move_range

    # 更新实体位置
    # 获取当前位置,只修改 Y 坐标
    current_pos = transform.position()
    transform.set_pos(lc.double3(current_pos.x, new_y, current_pos.z), False)
    render = re.world.RenderComponent(entity.get_component("RenderComponent"))
    move_mesh_vertices(app.ctx, cur_time * move_speed, render.mesh())


def move_mesh_vertices(
    ctx: re.world.RBCContext, time: float, mesh: re.world.MeshResource
):
    # get mesh data buffer
    mesh_array = np.ndarray(
        mesh.vertex_count() * 4, dtype=np.float32, buffer=mesh.pos_buffer()
    )
    right_side_indices = [8, 12, 24, 28]  # 右侧顶点的 x 坐标索引
    right_side_indices = [8, 12, 24, 28]  # 右侧顶点的 x 坐标索引

    # 基础 x 坐标和变形幅度
    base_x_right = 0.5
    base_x_left = -0.5
    amplitude = 0.3

    # 计算新的 x 偏移量
    offset = math.sin(time) * amplitude

    # 左侧顶点索引: 0, 1, 4, 5
    # 对应的x坐标在数组中的位置: 0*4=0, 1*4=4, 4*4=16, 5*4=20
    left_side_indices = [0, 4, 16, 20]  # 左侧顶点的 x 坐标索引

    # 更新右侧顶点的 x 坐标 (向右扩展)
    new_x_right = base_x_right + offset
    for idx in right_side_indices:
        mesh_array[idx] = new_x_right

    # 更新左侧顶点的 x 坐标 (向左扩展,即负方向)
    new_x_left = base_x_left - offset
    for idx in left_side_indices:
        mesh_array[idx] = new_x_left

    ctx.upload_mesh_data(mesh)


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

    app = rbc.app.App()  # rbc app singleton
    app.init(project_path=project_path, backend_name=args.backend)
    if not app.ctx:
        print("Context not Valid!")
        return

    app.init_display()
    if not app.display_cam:
        print("Display not Valid!")
        return

    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 0, -1), False)
    app.ctx.enable_camera_control()
    if not app.scene:
        print("Scene not Valid!")
        return

    entity = make_cube_mesh(app.scene)
    data = re.world.DataComponent(entity.add_component("DataComponent"))
    app.ctx.regist_callback("test_callback", test_callback)
    data.bind_event(re.world.DataComponentEventType.BeforeFrame, "test_callback")

    last_time = time.time()
    frame_index = 0
    tick_stage = re.world.TickStage.PathTracingPreview
    # app.run()
    while not app.ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        # only render one frame
        app.display_cam.set_frame_index(frame_index)
        app.ctx.tick(delta_time, tick_stage, True)
        frame_index = 0


if __name__ == "__main__":
    main()
