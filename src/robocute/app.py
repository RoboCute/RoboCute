from typing import Optional
import robocute.rbc_ext as re
from pathlib import Path
import robocute.rbc_ext.luisa as lc
import os
import time
import numpy as np

BUILTIN_PROGRAM_PATH = Path(
    os.path.dirname(__file__) + "/rbc_ext/_C"
)  # Built-In Runtime Path

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
    mat0 = re.world.MaterialResource()
    mat0.load_from_json(
        '{"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]}'
    )
    mat1 = re.world.MaterialResource()
    mat1.load_from_json(
        '{"type": "pbr", "specular_roughness": 0.5, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]}'
    )
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


# Python-Side Application
class App:
    _ctx: Optional[re.world.RBCContext] = None
    _initialized = False
    _instance: Optional["App"] = None
    _device = None
    _project: re.world.Project
    _resolution = lc.uint2
    _scene: Optional[re.world.Scene] = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def init(self, project_path: Path):
        world_path = project_path / "library"
        self.init_ctx()
        self.init_world(world_path)
        self.init_device()
        lc.init()
        self.init_project(project_path)

        self._initialized = True

    def init_ctx(self):
        self._ctx = re.world.RBCContext()

    def init_world(self, world_path: Path):
        if self._ctx is not None:
            self._ctx.init_world(str(world_path), str(world_path))

    def init_device(
        self, backend_name: str = "dx", program_path: Path = BUILTIN_PROGRAM_PATH
    ):
        shader_path = program_path / f"shader_build_{backend_name}"

        if self._ctx is not None:
            print(str(shader_path))
            self._ctx.init_device(backend_name, str(program_path), str(shader_path))
            self._ctx.init_render()

    def init_project(self, project_path: Path):
        self._project = re.world.Project()
        self._project.init(str(project_path / "assets"))
        self._project.scan_project()
        self._scene = self._project.import_scene("test_scene.scene", "")
        self._scene.install()

    def init_display(
        self, x: int = 1920, y: int = 1080, display_title: str = "py_window"
    ):
        self._resolution = lc.uint2(x, y)
        if not self._ctx:
            return

        self._ctx.init_display(display_title, self._resolution, True, True)
        print("display init")

    def initialized(self):
        return self._initialized

    def run(self):
        if not self._ctx or not self._scene:
            return

        display_cam = self._ctx.create_display_cam()
        transform = re.world.TransformComponent(
            display_cam.entity().get_component("TransformComponent")
        )
        display_cam.enable_camera()
        transform.set_pos(lc.double3(0, 0, -1), False)
        self._ctx.enable_camera_control()
        last_time = time.time()
        frame_index = 0
        image_index = 0
        tick_stage = re.world.TickStage.PathTracingPreview

        # entity = make_cube_mesh(self._scene)

        while not self._ctx.should_close():
            cur_time = time.time()
            delta_time = cur_time - last_time
            last_time = cur_time

            display_cam.set_frame_index(frame_index)

            if self._ctx.tick(delta_time, tick_stage, True):
                frame_index = 0
            else:
                frame_index += 1
