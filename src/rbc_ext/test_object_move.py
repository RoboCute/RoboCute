"""
物体移动测试脚本

本脚本用于测试 RoboCute 渲染引擎中的物体动态移动功能, 包括:
- 场景加载与渲染
- 动态网格创建
- 物体上下移动动画

用法:
    python test_object_move.py <scene_root_dir>

参数:
    scene_root_dir: 场景根目录路径, 应包含 library 和 assets 子目录

环境变量:
    RBC_RUNTIME_DIR: 运行时目录路径, 若未设置则自动检测

示例:
    python test_object_move.py C:/dev/RoboCute/samples/graphics

功能说明:
    - 初始化 RBC 上下文、渲染设备和显示窗口
    - 加载指定场景文件 (test_scene.scene)
    - 创建一个立方体网格实体
    - 在渲染循环中, 使立方体沿 Y 轴上下周期性移动
"""

import os
import sys
import time
from pathlib import Path
import rbc_ext.luisa as luisa
from rbc_ext.generated.world import *
import numpy as np
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


vertex_count = 8
"""单个立方体顶点数"""

triangle_count = 12
"""单个立方体三角形数"""

ctx = None
delta_time = None
def main():
    global ctx, delta_time
    """
    主函数: 初始化渲染环境并运行物体移动测试

    流程:
        1. 解析命令行参数, 获取场景根目录
        2. 初始化 RBC 上下文、世界、渲染设备和显示窗口
        3. 加载并安装场景
        4. 创建动态立方体网格实体
        5. 进入渲染循环, 使立方体沿 Y 轴上下周期性移动

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
    tick_stage = TickStage.RasterPreview
    
    # 创建立方体实体
    entity = make_cube_mesh(scene, ctx)
    
    # 获取变换组件用于后续移动
    transform = TransformComponent(entity.get_component("TransformComponent"))
    
    # 启用相机控制
    display_cam = ctx.create_display_cam()
    display_cam.enable_camera()
    ctx.enable_camera_control()
    
    while not ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        
        
        # 渲染一帧
        display_cam.set_frame_index(frame_index)
        
        ctx.tick(delta_time, tick_stage, True)
        frame_index = 0

last_time = time.time()
def test_callback(ptr):
    global last_time
    comp = DataComponent(ptr.handle)
    entity = comp.entity()
    transform = TransformComponent(entity.get_component("TransformComponent"))
    cur_time = time.time()
    # 移动动画参数
    move_speed = 2.0  # 移动速度
    move_range = 1.0  # 移动范围 (上下各 1 单位)
    base_y = -1.0     # Y 轴基准位置
    # 计算新的 Y 位置 (正弦波周期性移动)
    # 使用当前时间计算位置,实现平滑的周期性上下移动
    new_y = base_y + math.sin(cur_time * move_speed) * move_range
    
    # 更新实体位置
    # 获取当前位置,只修改 Y 坐标
    current_pos = transform.position()
    transform.set_pos(double3(current_pos.x, new_y, current_pos.z), False)
    render = RenderComponent(entity.get_component("RenderComponent"))
    move_mesh_vertices(ctx, cur_time * move_speed, render.mesh())
    
    
def make_cube_mesh(scene: Scene, ctx: RBCContext):
    """
    创建一个立方体动态网格实体

    创建的实体包含: 
        - TransformComponent: 控制实体位置和旋转
        - RenderComponent: 包含网格和材质渲染信息
        - MeshResource: 包含立方体网格数据
        - PBR 材质: 蓝色粗糙材质

    Returns:
        Entity: 创建的实体对象, 包含完整的渲染组件
    """
    mat0 = MaterialResource()
    mat0.load_from_json(
        '{"type": "pbr", "specular_roughness": 0.6, "weight_metallic": 0.2, "base_albedo": [0.2, 0.5, 0.9]}')
    mat_vector = capsule_vector()
    mat_vector.emplace_back(mat0._handle)
    
    entity = scene.add_entity()
    entity.set_name('moving_cube')
    
    trans = TransformComponent(entity.add_component("TransformComponent"))
    render = RenderComponent(entity.add_component("RenderComponent"))
    data = DataComponent(entity.add_component("DataComponent"))
    ctx.regist_callback('test_callback', test_callback)
    data.bind_event(DataComponentEventType.BeforeFrame, 'test_callback')
    
    # 设置初始位置
    trans.set_pos(double3(0, -1, 1), False)
    trans.set_rotation(float4(0, -1, 0, 0), False)
    
    # 创建网格
    cube_mesh = MeshResource()
    submesh_offsets = np.empty(shape=1, dtype=np.uint32)
    submesh_offsets[0] = 0
    
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

def move_mesh_vertices(ctx: RBCContext, time: float, mesh: MeshResource):
    """
    动态修改立方体网格顶点位置,使右侧顶点沿 X 轴周期性移动

    通过正弦波控制右侧顶点(2, 3, 6, 7)的 X 坐标,产生呼吸/变形动画效果

    Args:
        ctx: RBC 上下文
        time: 当前时间(用于计算正弦波)
        mesh: 网格资源对象
    """
    # 获取网格数据缓冲区
    mesh_array = np.ndarray(
        mesh.vertex_count() * 4,
        dtype=np.float32,
        buffer=mesh.pos_buffer()
    )
    
    # 顶点数据开始位置,每个顶点4个float(x,y,z,w)
    # 右侧顶点索引: 2, 3, 6, 7
    # 对应的x坐标在数组中的位置: 2*4=8, 3*4=12, 6*4=24, 7*4=28
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
        

def create_mesh_array(mesh_array):
    """
    生成立方体的顶点数据和索引数据

    数据布局:
        - 顶点数据: 每个顶点4个float(x, y, z, w), 共8个顶点
        - 索引数据: 每个三角形3个uint32索引, 共12个三角形

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

    def push_vec4(x, y, z):
        """向顶点缓冲区添加一个 float4 顶点"""
        nonlocal size
        vec = float4(x, y, z, 0)
        for i in range(4):
            vertex_arr[size + i] = vec[i]
        size += 4

    def push_indices(idx: int):
        """向索引缓冲区添加一个顶点索引"""
        nonlocal size
        indices_arr[size] = idx
        size += 1

    # 添加8个立方体顶点
    push_vec4(-0.5, -0.5, -0.5)  # 0: 左下后
    push_vec4(-0.5, -0.5, 0.5)   # 1: 左下前
    push_vec4(0.5, -0.5, -0.5)   # 2: 右下后
    push_vec4(0.5, -0.5, 0.5)    # 3: 右下前
    push_vec4(-0.5, 0.5, -0.5)   # 4: 左上后
    push_vec4(-0.5, 0.5, 0.5)    # 5: 左上前
    push_vec4(0.5, 0.5, -0.5)    # 6: 右上后
    push_vec4(0.5, 0.5, 0.5)     # 7: 右上前

    size = 0

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


if __name__ == "__main__":
    main()
    del ctx
