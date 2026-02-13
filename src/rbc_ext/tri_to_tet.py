import argparse
import sys
import warnings
from pathlib import Path
from typing import Optional, Tuple, List, Dict, Any, Callable
import numpy as np
from rbc_ext._C.test_py_codegen import *


def tetrahedralize(vertices: np.ndarray, faces: np.ndarray,
                   cell_size=None,
                   radius_edge_ratio=2.0
                   ) -> Tuple[np.ndarray, np.ndarray]:

    def float4_array_to_float3_array(array):
        array = np.asarray(array)
        if array.shape[-1] != 4:
            raise ValueError(
                f"Expected last dimension to be 4, got {array.shape[-1]}")
        return array[..., :3]
    """
    使用 tetgen 将三角面模型转换为四面体网格

    Args:
        vertices: 输入顶点数组，形状为 (N, 3)，float32 类型
        faces: 输入面数组，形状为 (M, 3)，int32 类型（三角形面）
        cell_size: 控制生成四面体的大小（默认自动计算）
        radius_edge_ratio: 半径边比，控制四面体质量（默认 2.0）

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
