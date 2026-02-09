"""
图形场景测试脚本

本脚本用于测试 RoboCute 渲染引擎的图形场景功能, 包括: 
- 场景加载与渲染
- 动态网格创建与管理
- 几何缓冲区导出(深度、法线、发射、反照率)
- 交互式相机控制

用法:
    python test_graphics_scene.py <scene_root_dir>

参数:
    scene_root_dir: 场景根目录路径, 应包含 library 和 assets 子目录

环境变量:
    RBC_RUNTIME_DIR: 运行时目录路径, 若未设置则自动检测

示例:
    python test_graphics_scene.py C:/dev/RoboCute/samples/graphics

功能说明:
    - 初始化 RBC 上下文、渲染设备和显示窗口
    - 加载指定场景文件 (test_scene.scene)
    - 创建动态立方体网格实体(包含两个子网格和两种材质)
    - 运行渲染循环, 支持相机控制和实时预览
    - 当 EXPORT=True 时, 可导出几何缓冲区数据为图片
"""

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
"""是否导出几何缓冲区数据为图片, 设为 True 时启用导出功能"""

vertex_count = 16
"""网格顶点总数(两个立方体, 每个8个顶点)"""

triangle_count = 24
"""网格三角形总数(两个立方体, 每个12个三角形)"""


def main():
    """
    主函数: 初始化渲染环境并运行场景测试

    流程:
        1. 解析命令行参数, 获取场景根目录
        2. 初始化 RBC 上下文、世界、渲染设备和显示窗口
        3. 加载并安装场景
        4. 创建动态立方体网格实体
        5. 进入渲染循环, 处理帧更新和相机控制
        6. 当 EXPORT=True 时, 导出几何缓冲区数据

    退出条件:
        - 用户关闭窗口 (ctx.should_close())
        - 发生异常错误
    """
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
    project.scan_project()
    resolution = uint2(1920, 1080)
    ctx.init_display("py_window", resolution, True, True)
    scene = project.import_scene("test_scene.scene", "")
    scene.install()
    last_time = time.time()
    frame_index = 0
    image_index = 0
    tick_stage = TickStage.PathTracingPreview
    entity = make_cube_mesh(scene)
    @luisa.func
    def write_buffer_vec3_to_img(buffer, element_offset, img):
        """
        Luisa kernel: 将 float3 数据从缓冲区写入图像

        用于将几何缓冲区中的 vec3 数据 (如 normal albedo) 可视化为图像

        Args:
            buffer: 源数据缓冲区
            element_offset: 在缓冲区中的起始偏移量
            img: 目标图像
        """
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
        """
        Luisa kernel: 将 float 数据从缓冲区写入图像

        用于将几何缓冲区中的标量数据(如深度)可视化为灰度图像

        Args:
            buffer: 源数据缓冲区
            scale: 缩放因子, 用于调整数据范围到可视范围
            element_offset: 在缓冲区中的起始偏移量
            img: 目标图像
        """
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


def make_cube_mesh(scene: Scene):
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
    mat0 = MaterialResource()
    mat0.load_from_json(
        '{"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]}')
    mat1 = MaterialResource()
    mat1.load_from_json(
        '{"type": "pbr", "specular_roughness": 0.5, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]}')
    mat_vector = capsule_vector()
    mat_vector.emplace_back(mat0._handle)
    mat_vector.emplace_back(mat1._handle)
    entity = scene.add_entity()
    # Test entity by name
    entity.set_name('test_cube')
    entity = scene.get_entity_by_name('test_cube')
    assert entity._handle is not None
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
        """向顶点缓冲区添加一个 float4 顶点

        Args:
            x, y, z: 顶点坐标分量
        """
        nonlocal size, offset, scale
        vec = float4(x, y, z, 0) * scale + offset
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
        push_vec4(-0.5, -0.5, 0.5)   # 1: 左下前
        push_vec4(0.5, -0.5, -0.5)   # 2: 右下后
        push_vec4(0.5, -0.5, 0.5)    # 3: 右下前
        push_vec4(-0.5, 0.5, -0.5)   # 4: 左上后
        push_vec4(-0.5, 0.5, 0.5)    # 5: 左上前
        push_vec4(0.5, 0.5, -0.5)    # 6: 右上后
        push_vec4(0.5, 0.5, 0.5)     # 7: 右上前

    push_vert()
    last_vert_size = size
    offset = float4(0, 1, 0, 0)
    scale = float4(0.4, 0.4, 0.4, 0)
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


if __name__ == "__main__":
    main()
