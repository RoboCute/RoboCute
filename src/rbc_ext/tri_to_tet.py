import argparse
import sys
import warnings
from pathlib import Path
from typing import Optional, Tuple, List, Dict, Any, Callable
import numpy as np
from rbc_ext._C.test_py_codegen import *


def tetrahedralize(vertices: np.ndarray, faces: np.ndarray,
                   cell_size = None,
                   radius_edge_ratio = 2.0,
                   facet_distance = None,
                   facet_angle = 25.0,
                   edge_size = None
                   ) -> Tuple[np.ndarray, np.ndarray]:

    def float4_array_to_float3_array(array):
        array = np.asarray(array)
        if array.shape[-1] != 4:
            raise ValueError(f"Expected last dimension to be 4, got {array.shape[-1]}")
        return array[..., :3]
    """
    使用 tetgen 将三角面模型转换为四面体网格

    Args:
        vertices: 输入顶点数组，形状为 (N, 3)，float32 类型
        faces: 输入面数组，形状为 (M, 3)，int32 类型（三角形面）
        cell_size: 控制生成四面体的大小（默认自动计算）
        radius_edge_ratio: 半径边比，控制四面体质量（默认 2.0）
        facet_distance: 表面逼近精度（默认自动计算）
        facet_angle: 表面三角形最小角度（默认 25.0）
        edge_size: 边长控制（默认自动计算）

    Returns:
        Tuple[np.ndarray, np.ndarray]: (tet_vertices, tet_cells)
            - tet_vertices: 四面体顶点数组，形状为 (N, 3)
            - tet_cells: 四面体单元数组，形状为 (M, 4)，每个四面体包含4个顶点索引

    Raises:
        ImportError: 如果 tetgen 未安装
        RuntimeError: 如果四面体化失败
    """
    try:
        import tetgen
    except ImportError:
        raise ImportError(
            "tetgen is required for tetrahedralization. "
            "Please install it with: pip install tetgen"
        )

    # 确保输入是 numpy 数组且类型正确
    vertices = np.asarray(vertices, dtype=np.float64)
    faces = np.asarray(faces, dtype=np.int32)

    # 验证输入形状
    if vertices.ndim == 2 or vertices.shape[1] == 4:
        vertices = float4_array_to_float3_array(vertices)

    if vertices.ndim != 2 or vertices.shape[1] != 3:
        raise ValueError(
            f"vertices must have shape (N, 3), got {vertices.shape}")
    if faces.ndim != 2 or faces.shape[1] != 3:
        raise ValueError(
            f"faces must have shape (M, 3) for triangles, got {faces.shape}")

    # 自动计算默认参数
    if cell_size is None:
        bbox_min = np.min(vertices, axis=0)
        bbox_max = np.max(vertices, axis=0)
        bbox_size = np.max(bbox_max - bbox_min)
        cell_size = bbox_size / 10.0

    try:
        # 创建 tetgen 网格
        tet = tetgen.TetGen(vertices, faces)
        
        # 构建四面体网格
        # switches 参数控制 tetgen 的行为:
        # -p: 从分段平面网格生成四面体网格
        # -q: 指定半径-边比质量阈值
        # -a: 最大四面体体积约束
        # -Y: 禁止在表面添加 Steiner 点（保持输入表面）
        switches = f"pq{radius_edge_ratio}Y"
        if cell_size is not None:
            max_volume = cell_size ** 3
            switches += f"a{max_volume}"
        
        tet.tetrahedralize(switches=switches)
        
        # 提取结果
        # tet.node 是节点数组，包含所有顶点
        # tet.elem 是元素数组，每个元素包含4个顶点索引
        tet_vertices = tet.node.astype(np.float32)
        tet_cells = tet.elem.astype(np.int32)
        
    except Exception as e:
        raise RuntimeError(f"Tetrahedralization failed: {e}")

    return tet_vertices, tet_cells


vertex_count = 8

triangle_count = 12


def create_mesh_array():
    vertex_arr = np.zeros(shape=vertex_count * 4, dtype=np.float32)
    indices_arr = np.zeros(shape=triangle_count * 3, dtype=np.uint32)
    size = 0
    offset = float4(0)
    scale = float4(1)

    def push_vec3(x, y, z):
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
        push_vec3(-0.5, -0.5, -0.5)  # 0: 左下后
        push_vec3(-0.5, -0.5, 0.5)   # 1: 左下前
        push_vec3(0.5, -0.5, -0.5)   # 2: 右下后
        push_vec3(0.5, -0.5, 0.5)    # 3: 右下前
        push_vec3(-0.5, 0.5, -0.5)   # 4: 左上后
        push_vec3(-0.5, 0.5, 0.5)    # 5: 左上前
        push_vec3(0.5, 0.5, -0.5)    # 6: 右上后
        push_vec3(0.5, 0.5, 0.5)     # 7: 右上前

    push_vert()
    # last_vert_size = size
    # offset = float4(0, 1, 0, 0)
    # scale = float4(0.4, 0.4, 0.4, 0)
    # push_vert()
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
    # last_index_size = size
    # # index size to triangle size
    # last_tri_size = last_index_size // 3
    # push_cube_triangles()
    # for i in range(last_index_size, size):
    #     indices_arr[i] += last_vert_size // 4
    return vertex_arr, indices_arr


def main():
    vertex_arr, indices_arr = create_mesh_array()
    vertices = vertex_arr.reshape(-1, 4)
    faces = indices_arr.reshape(-1, 3)
    tet_vertices, tet_cells = tetrahedralize(vertices, faces)
    print(tet_vertices)
    print(tet_cells)


if __name__ == "__main__":
    main()
    print('finish')
