import os
import sys
import time
from pathlib import Path
import numpy as np
import rbc_ext._C.pyuipc as pyuipc
from rbc_ext.generated.world import *
from rbc_ext._C.pyuipc import Logger
from rbc_ext._C.pyuipc import Matrix4x4
from rbc_ext._C.pyuipc import view
from rbc_ext._C.pyuipc import Engine, World, Scene, SceneIO
from rbc_ext._C.pyuipc.geometry import SimplicialComplex, SimplicialComplexIO
from rbc_ext._C.pyuipc.geometry import SpreadSheetIO
from rbc_ext._C.pyuipc.geometry import label_surface, label_triangle_orient, flip_inward_triangles
from rbc_ext._C.pyuipc.geometry import ground
from rbc_ext._C.pyuipc.constitution import StableNeoHookean, AffineBodyConstitution, ElasticModuli


class UIPCDemo:
    """
    UIPC物理模拟演示类
    
    使用 pyuipc 进行物理模拟，展示了如何:
    - 创建 pyuipc 引擎和世界
    - 设置场景配置
    - 创建 FEM (有限元) 和 ABD (仿射体动力学) 物体
    - 运行模拟并更新网格顶点位置
    """
    
    # Start writen by AGENT
    
    def __init__(self):
        """初始化 UIPC 模拟环境"""
        # 设置日志级别
        Logger.set_level(Logger.Level.Info)
        
        # 创建引擎 (使用 cuda 后端)
        self.workspace = str(Path(__file__).parent / '.uipc')
        os.makedirs(self.workspace, exist_ok=True)
        
        module_dir = str(Path(__file__).parent / '_C')
        config = pyuipc.config()
        config["module_dir"] = module_dir
        pyuipc.init(config)
        
        self.engine = Engine("cuda", self.workspace)
        self.world = World(self.engine)
        
        # 创建场景配置
        config = Scene.default_config()
        self.scene = Scene(config)
        
        # 创建材质本构
        self.snk = StableNeoHookean()  # 稳定的 Neo-Hookean 弹性材料
        self.abd = AffineBodyConstitution()  # 仿射体本构
        
        # 将本构添加到场景的本构表中
        self.scene.constitution_tabular().insert(self.snk)
        self.scene.constitution_tabular().insert(self.abd)
        
        # 设置默认接触模型
        self.scene.contact_tabular().default_model(0.5, 1e9)
        self.default_element = self.scene.contact_tabular().default_element()
        
        # 创建立方体网格
        self._create_cube_mesh()
        
        # 初始化世界
        self.world.init(self.scene)
    
    def _create_cube_mesh(self):
        """创建立方体网格并添加到场景"""
        # 创建单位变换矩阵
        pre_trans = Matrix4x4.Identity()
        
        # 创建立方体的顶点 (8个顶点)
        # 1-based to 0-based index mapping:
        # 1->0, 2->1, 3->2, 4->3, 5->4, 6->5, 7->6, 8->7
        vertices = np.array([
            [-0.5,  0.5, -0.5],  # 0 (was 1)
            [0.5,  0.5, -0.5],  # 1 (was 2)
            [0.5,  0.5,  0.5],  # 2 (was 3)
            [-0.5,  0.5,  0.5],  # 3 (was 4)
            [-0.5, -0.5, -0.5],  # 4 (was 5)
            [0.5, -0.5, -0.5],  # 5 (was 6)
            [0.5, -0.5,  0.5],  # 6 (was 7)
            [-0.5, -0.5,  0.5],  # 7 (was 8)
        ], dtype=np.float64)
        
        # 创建立方体的四面体 (5个四面体，将 1-based 索引转换为 0-based)
        # 原索引: 1->0, 2->1, 3->2, 4->3, 5->4, 6->5, 7->6, 8->7
        tets = np.array([
            [0, 7, 4, 5],  # was [1, 8, 5, 6]
            [7, 0, 3, 2],  # was [8, 1, 4, 3]
            [2, 5, 6, 7],  # was [3, 6, 7, 8]
            [5, 2, 1, 0],  # was [6, 3, 2, 1]
            [7, 2, 5, 0],  # was [8, 3, 6, 1]
        ], dtype=np.int32)
        
        # 使用 tetmesh 创建 SimplicialComplex
        cube = pyuipc.geometry.tetmesh(vertices, tets)
        
        # 处理表面 (标记表面三角形并调整方向)
        pyuipc.geometry.label_surface(cube)
        pyuipc.geometry.label_triangle_orient(cube)
        cube = pyuipc.geometry.flip_inward_triangles(cube)
        
        # 创建 FEM 立方体 (使用弹性材料)
        fem_cube = cube.copy()
        moduli = ElasticModuli.youngs_poisson(1e5, 0.49)  # 杨氏模量 1e5 Pa, 泊松比 0.49
        self.snk.apply_to(fem_cube, moduli)
        self.default_element.apply_to(fem_cube)
        
        # 创建 ABD 立方体 (刚体)
        abd_cube = cube.copy()
        self.abd.apply_to(abd_cube, 1e8)  # 高刚度模拟刚体
        self.default_element.apply_to(abd_cube)
        
        # 创建物体并添加几何体
        obj = self.scene.objects().create("object")
        N = 3  # 创建3个堆叠的立方体
        
        for i in range(N):
            geo = None
            if i % 2 == 0:
                # 偶数索引使用 FEM
                geo = fem_cube.copy()
                pos_v = view(geo.positions())
                for j in range(len(pos_v)):
                    pos_v[j][1] += 1.2 * i  # 在 Y 方向堆叠
            else:
                # 奇数索引使用 ABD
                geo = abd_cube.copy()
                pos_v = view(geo.positions())
                for j in range(len(pos_v)):
                    pos_v[j][1] += 1.2 * i  # 在 Y 方向堆叠
            obj.geometries().create(geo)
        
        # 添加地面
        g = pyuipc.geometry.ground(-1.2)
        obj.geometries().create(g)
    
        # Start writen by AGENT
    def move_mesh_vertices(self, mesh_array: np.ndarray, vertex_count: int):
        """
        运行一帧模拟并更新网格顶点位置
        
        Args:
            mesh_array: 网格顶点数组
            vertex_count: 顶点数量
        """
        # 运行一帧模拟
        self.world.advance()
        self.world.retrieve()
        
        # 使用 SceneIO 获取模拟后的表面网格顶点位置
        sio = SceneIO(self.scene)
        surface = sio.simplicial_surface()
        positions = view(surface.positions())
        print(positions)
        
        # 更新网格顶点位置
        if mesh_array is not None and positions is not None:
            vertex_count = min(len(positions), vertex_count)
            for i in range(vertex_count):
                mesh_array[i * 4 + 0] = float(positions[i][0])
                mesh_array[i * 4 + 1] = float(positions[i][1])
                mesh_array[i * 4 + 2] = float(positions[i][2])
                mesh_array[i * 4 + 3] = 0.0  # w 分量
        # End writen by AGENT
    
    def __del__(self):
        """清理资源"""
        del self.world
        del self.engine
    
    # End writen by AGENT

if __name__ == "__main__":
    uipc_demo = UIPCDemo()
    uipc_demo.move_mesh_vertices(None, None)