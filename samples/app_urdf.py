#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
URDF 机械臂场景示例

本示例演示如何:
1. 创建 URDF 格式的机械臂描述文件（6自由度 myCobot 280 风格）
2. 解析 URDF 并创建对应的可视化实体
3. 实现简单的关节动画，展示机械臂运动
4. 渲染 URDF 场景到图像文件

新增案例 - 渲染 URDF 场景 (case_render_urdf_scene):
    使用 -r 或 --render 参数运行渲染案例，将 URDF 机械臂场景渲染为图像。
    支持自定义输出路径 --output-image <path>

参考:
- app_graphics_scene.py (RoboCute 渲染)
- rbc/tests/test_urdf/test_urdf_robo.cpp (URDF 结构)

依赖安装 (如果缺少包):
    uv pip install numpy
    # 或: pip install numpy

运行示例:
    # 交互式可视化
    python samples/app_urdf.py -p ./my_project
    
    # 渲染案例 - 输出到默认路径
    python samples/app_urdf.py -p ./my_project -r
    
    # 渲染案例 - 指定输出路径
    python samples/app_urdf.py -p ./my_project -r --output-image ./output/robot.png
"""
import os
import sys
import time
import math
from pathlib import Path
import argparse
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass

# 尝试导入 numpy，如果失败则提示安装
try:
    import numpy as np
except ImportError:
    print("错误: 缺少 numpy 包。请运行以下命令安装:")
    print("  uv pip install numpy")
    print("  # 或: pip install numpy")
    sys.exit(1)

import xml.etree.ElementTree as ET

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
import samples.mat_builtin as mat


app: rbc.app.App = None
"""全局 RoboCute app 单例"""


@dataclass
class JointInfo:
    """关节信息数据类"""
    name: str
    type: str  # revolute, continuous, fixed, prismatic
    parent: str
    child: str
    origin_xyz: Tuple[float, float, float]
    origin_rpy: Tuple[float, float, float]
    axis: Tuple[float, float, float]
    lower: float = 0.0
    upper: float = 0.0
    effort: float = 0.0
    velocity: float = 0.0


@dataclass
class LinkInfo:
    """连杆信息数据类"""
    name: str
    geometry_type: str  # cylinder, box, sphere
    geometry_params: Dict
    material_color: Tuple[float, float, float, float]
    mass: float = 0.0
    inertia: Optional[Dict] = None


# ============================================================
# URDF 创建和解析
# ============================================================

# 机械臂缩放因子（默认 3.0 倍，可根据需要调整）
# 修改此值可改变机械臂大小：
#   1.0 = 原始大小
#   2.0 = 2倍大小
#   3.0 = 3倍大小（默认）
ROBOT_SCALE = 10.


def create_mycobot_280_urdf(scale: float = ROBOT_SCALE) -> str:
    """
    创建 myCobot 280 风格的 6 自由度机械臂 URDF

    【连杆结构】
    - base_link: 底座，圆柱形，直径 0.08m，高度 0.05m
    - link1: 旋转基座，圆柱形
    - link2: 下臂，长方体
    - link3: 上臂，长方体
    - link4: 手腕旋转部，圆柱形
    - link5: 手腕俯仰部，圆柱形
    - link6: 末端执行器安装法兰，圆柱形

    【关节配置】
    - joint1: 基座旋转，绕 Z 轴，-180° ~ +180°
    - joint2: 肩关节，绕 Y 轴，-150° ~ +150°
    - joint3: 肘关节，绕 Y 轴，-150° ~ +150°
    - joint4: 手腕旋转，绕 Z 轴，-180° ~ +180°
    - joint5: 手腕俯仰，绕 Y 轴，-100° ~ +100°
    - joint6: 末端旋转，绕 Z 轴，-180° ~ +180°

    Args:
        scale: 缩放因子，默认 3.0（放大3倍）

    Returns:
        URDF XML 字符串
    """
    s = scale  # 简写，方便使用
    return f"""<?xml version="1.0"?>
<robot name="mycobot_280">
  <!-- 材质定义 -->
  <material name="white">
    <color rgba="1 1 1 1"/>
  </material>
  <material name="blue">
    <color rgba="0.2 0.4 0.8 1"/>
  </material>
  <material name="silver">
    <color rgba="0.75 0.75 0.75 1"/>
  </material>
  <material name="orange">
    <color rgba="1.0 0.5 0.0 1"/>
  </material>
  
  <!-- 底座连杆 -->
  <link name="base_link">
    <visual>
      <geometry>
        <cylinder radius="{0.04 * s}" length="{0.05 * s}"/>
      </geometry>
      <material name="silver"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="{0.04 * s}" length="{0.05 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.3 * s * s * s}"/>
      <inertia ixx="{0.0001 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.0001 * s * s * s * s}" iyz="0" izz="{0.0002 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆1：旋转基座 -->
  <link name="link1">
    <visual>
      <geometry>
        <cylinder radius="{0.035 * s}" length="{0.06 * s}"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="{0.035 * s}" length="{0.06 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.15 * s * s * s}"/>
      <inertia ixx="{0.00005 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.00005 * s * s * s * s}" iyz="0" izz="{0.0001 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆2：下臂 -->
  <link name="link2">
    <visual>
      <geometry>
        <box size="{0.04 * s} {0.03 * s} {0.12 * s}"/>
      </geometry>
      <material name="white"/>
    </visual>
    <collision>
      <geometry>
        <box size="{0.04 * s} {0.03 * s} {0.12 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.12 * s * s * s}"/>
      <origin xyz="0 0 {0.06 * s}"/>
      <inertia ixx="{0.00015 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.00018 * s * s * s * s}" iyz="0" izz="{0.00005 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆3：上臂 -->
  <link name="link3">
    <visual>
      <geometry>
        <box size="{0.035 * s} {0.025 * s} {0.10 * s}"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <box size="{0.035 * s} {0.025 * s} {0.10 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.10 * s * s * s}"/>
      <origin xyz="0 0 {0.05 * s}"/>
      <inertia ixx="{0.00009 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.00011 * s * s * s * s}" iyz="0" izz="{0.00003 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆4：手腕旋转部 -->
  <link name="link4">
    <visual>
      <geometry>
        <cylinder radius="{0.025 * s}" length="{0.04 * s}"/>
      </geometry>
      <material name="silver"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="{0.025 * s}" length="{0.04 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.08 * s * s * s}"/>
      <inertia ixx="{0.00002 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.00002 * s * s * s * s}" iyz="0" izz="{0.00003 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆5：手腕俯仰部 -->
  <link name="link5">
    <visual>
      <geometry>
        <cylinder radius="{0.02 * s}" length="{0.035 * s}"/>
      </geometry>
      <material name="blue"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="{0.02 * s}" length="{0.035 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.05 * s * s * s}"/>
      <inertia ixx="{0.00001 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.00001 * s * s * s * s}" iyz="0" izz="{0.00001 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 连杆6：末端法兰 -->
  <link name="link6">
    <visual>
      <geometry>
        <cylinder radius="{0.015 * s}" length="{0.02 * s}"/>
      </geometry>
      <material name="orange"/>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="{0.015 * s}" length="{0.02 * s}"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="{0.03 * s * s * s}"/>
      <inertia ixx="{0.000003 * s * s * s * s}" ixy="0" ixz="0" iyy="{0.000003 * s * s * s * s}" iyz="0" izz="{0.000005 * s * s * s * s}"/>
    </inertial>
  </link>
  
  <!-- 关节1：基座旋转，绕 Z 轴 -->
  <joint name="joint1" type="revolute">
    <parent link="base_link"/>
    <child link="link1"/>
    <origin xyz="0 0 {0.055 * s}" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="{5 * s * s * s}" velocity="3.14"/>
  </joint>
  
  <!-- 关节2：肩关节，绕 Y 轴 -->
  <joint name="joint2" type="revolute">
    <parent link="link1"/>
    <child link="link2"/>
    <origin xyz="0 0 {0.09 * s}" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-2.62" upper="2.62" effort="{5 * s * s * s}" velocity="3.14"/>
  </joint>
  
  <!-- 关节3：肘关节，绕 Y 轴 -->
  <joint name="joint3" type="revolute">
    <parent link="link2"/>
    <child link="link3"/>
    <origin xyz="0 0 {0.13 * s}" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-2.62" upper="2.62" effort="{3 * s * s * s}" velocity="3.14"/>
  </joint>
  
  <!-- 关节4：手腕旋转，绕 Z 轴 -->
  <joint name="joint4" type="revolute">
    <parent link="link3"/>
    <child link="link4"/>
    <origin xyz="0 0 {0.09 * s}" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="{2 * s * s * s}" velocity="6.28"/>
  </joint>
  
  <!-- 关节5：手腕俯仰，绕 Y 轴 -->
  <joint name="joint5" type="revolute">
    <parent link="link4"/>
    <child link="link5"/>
    <origin xyz="0 0 {0.04 * s}" rpy="0 0 0"/>
    <axis xyz="0 1 0"/>
    <limit lower="-1.75" upper="1.75" effort="{2 * s * s * s}" velocity="6.28"/>
  </joint>
  
  <!-- 关节6：末端旋转，绕 Z 轴 -->
  <joint name="joint6" type="revolute">
    <parent link="link5"/>
    <child link="link6"/>
    <origin xyz="0 0 {0.0375 * s}" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="{1 * s * s * s}" velocity="6.28"/>
  </joint>
</robot>
"""


def parse_urdf(urdf_xml: str) -> Tuple[Dict[str, LinkInfo], Dict[str, JointInfo]]:
    """
    解析 URDF XML 字符串

    Args:
        urdf_xml: URDF XML 字符串

    Returns:
        (links, joints) 字典元组
    """
    root = ET.fromstring(urdf_xml)

    links = {}
    joints = {}

    # 解析连杆
    for link_elem in root.findall('link'):
        name = link_elem.get('name')
        visual = link_elem.find('visual')

        geometry_type = 'cylinder'
        geometry_params = {}
        material_color = (0.5, 0.5, 0.5, 1.0)

        if visual is not None:
            geometry = visual.find('geometry')
            material = visual.find('material')

            if geometry is not None:
                if geometry.find('cylinder') is not None:
                    geometry_type = 'cylinder'
                    cyl = geometry.find('cylinder')
                    geometry_params['radius'] = float(cyl.get('radius', 0.1))
                    geometry_params['length'] = float(cyl.get('length', 0.1))
                elif geometry.find('box') is not None:
                    geometry_type = 'box'
                    box = geometry.find('box')
                    size_str = box.get('size', '0.1 0.1 0.1')
                    geometry_params['size'] = [
                        float(x) for x in size_str.split()]
                elif geometry.find('sphere') is not None:
                    geometry_type = 'sphere'
                    sph = geometry.find('sphere')
                    geometry_params['radius'] = float(sph.get('radius', 0.1))

            if material is not None:
                color = material.find('color')
                if color is not None:
                    rgba_str = color.get('rgba', '0.5 0.5 0.5 1')
                    material_color = tuple(float(x) for x in rgba_str.split())

        # 解析质量和惯性
        mass = 0.0
        inertial = link_elem.find('inertial')
        if inertial is not None:
            mass_elem = inertial.find('mass')
            if mass_elem is not None:
                mass = float(mass_elem.get('value', 0))

        links[name] = LinkInfo(
            name=name,
            geometry_type=geometry_type,
            geometry_params=geometry_params,
            material_color=material_color,
            mass=mass
        )

    # 解析关节
    for joint_elem in root.findall('joint'):
        name = joint_elem.get('name')
        joint_type = joint_elem.get('type', 'fixed')

        parent = joint_elem.find('parent').get(
            'link') if joint_elem.find('parent') is not None else ''
        child = joint_elem.find('child').get(
            'link') if joint_elem.find('child') is not None else ''

        origin = joint_elem.find('origin')
        xyz = (0.0, 0.0, 0.0)
        rpy = (0.0, 0.0, 0.0)
        if origin is not None:
            xyz_str = origin.get('xyz', '0 0 0')
            xyz = tuple(float(x) for x in xyz_str.split())
            rpy_str = origin.get('rpy', '0 0 0')
            rpy = tuple(float(x) for x in rpy_str.split())

        axis_elem = joint_elem.find('axis')
        axis = (0.0, 0.0, 1.0)
        if axis_elem is not None:
            axis_str = axis_elem.get('xyz', '0 0 1')
            axis = tuple(float(x) for x in axis_str.split())

        limit_elem = joint_elem.find('limit')
        lower, upper, effort, velocity = -1.0, 1.0, 1.0, 1.0
        if limit_elem is not None:
            lower = float(limit_elem.get('lower', -1.0))
            upper = float(limit_elem.get('upper', 1.0))
            effort = float(limit_elem.get('effort', 1.0))
            velocity = float(limit_elem.get('velocity', 1.0))

        joints[name] = JointInfo(
            name=name,
            type=joint_type,
            parent=parent,
            child=child,
            origin_xyz=xyz,
            origin_rpy=rpy,
            axis=axis,
            lower=lower,
            upper=upper,
            effort=effort,
            velocity=velocity
        )

    return links, joints


# ============================================================
# 网格生成
# ============================================================

def create_cylinder_mesh(radius: float, length: float) -> Tuple[np.ndarray, np.ndarray]:
    """
    创建圆柱体网格

    Args:
        radius: 圆柱半径
        length: 圆柱高度

    Returns:
        (vertices, indices) 元组
        vertices: Nx4 的顶点数组 (x, y, z, w)
        indices: Mx3 的三角形索引数组
    """
    # 圆柱体分段数
    segments = 16

    vertices = []
    indices = []

    half_length = length / 2

    # 底部圆心 (索引 0)
    vertices.append([0, -half_length, 0, 1])
    # 顶部圆心 (索引 1)
    vertices.append([0, half_length, 0, 1])

    # 底部圆周顶点 (索引 2 ~ segments+1)
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        vertices.append([x, -half_length, z, 1])

    # 顶部圆周顶点 (索引 segments+2 ~ 2*segments+1)
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        vertices.append([x, half_length, z, 1])

    # 底部三角形 (指向圆心)
    for i in range(segments):
        next_i = (i + 1) % segments
        indices.append([0, 2 + i, 2 + next_i])

    # 顶部三角形 (指向圆心)
    top_start = 2 + segments
    for i in range(segments):
        next_i = (i + 1) % segments
        indices.append([1, top_start + next_i, top_start + i])

    # 侧面矩形（分解为两个三角形）
    for i in range(segments):
        next_i = (i + 1) % segments
        bottom_i = 2 + i
        bottom_next = 2 + next_i
        top_i = top_start + i
        top_next = top_start + next_i

        indices.append([bottom_i, top_i, bottom_next])
        indices.append([bottom_next, top_i, top_next])

    return np.array(vertices, dtype=np.float32), np.array(indices, dtype=np.uint32)


def create_box_mesh(size: List[float]) -> Tuple[np.ndarray, np.ndarray]:
    """
    创建长方体网格

    Args:
        size: [x, y, z] 尺寸

    Returns:
        (vertices, indices) 元组
    """
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2

    # 8 个顶点
    vertices = np.array([
        [-hx, -hy, -hz, 1],  # 0
        [hx, -hy, -hz, 1],  # 1
        [hx,  hy, -hz, 1],  # 2
        [-hx,  hy, -hz, 1],  # 3
        [-hx, -hy,  hz, 1],  # 4
        [hx, -hy,  hz, 1],  # 5
        [hx,  hy,  hz, 1],  # 6
        [-hx,  hy,  hz, 1],  # 7
    ], dtype=np.float32)

    # 12 个三角形（6 个面）
    indices = np.array([
        # 底面 (y = -hy)
        [0, 1, 4], [1, 5, 4],
        # 顶面 (y = hy)
        [3, 2, 7], [2, 6, 7],
        # 前面 (z = hz)
        [4, 5, 7], [5, 6, 7],
        # 后面 (z = -hz)
        [0, 3, 1], [1, 3, 2],
        # 左面 (x = -hx)
        [0, 4, 3], [3, 4, 7],
        # 右面 (x = hx)
        [1, 2, 5], [2, 6, 5],
    ], dtype=np.uint32)

    return vertices, indices


# ============================================================
# 实体创建
# ============================================================

class RobotArmVisualizer:
    """机械臂可视化器"""

    def __init__(self, scene: re.world.Scene):
        self.scene = scene
        self.link_entities: Dict[str, re.world.Entity] = {}
        self.link_transforms: Dict[str, re.world.TransformComponent] = {}
        self.joints: Dict[str, JointInfo] = {}
        self.links: Dict[str, LinkInfo] = {}

    def build_from_urdf(self, urdf_xml: str):
        """从 URDF 构建机械臂"""
        self.links, self.joints = parse_urdf(urdf_xml)

        # 构建父子关系图
        self._build_hierarchy()

        # 为每个连杆创建实体
        for link_name, link_info in self.links.items():
            self._create_link_entity(link_name, link_info)

        # 设置初始关节变换
        self._apply_joint_transforms()

    def _build_hierarchy(self):
        """构建连杆层级关系"""
        self.parent_map = {}  # child -> parent
        self.children_map = {}  # parent -> [children]

        for joint_name, joint_info in self.joints.items():
            self.parent_map[joint_info.child] = (joint_info.parent, joint_info)
            if joint_info.parent not in self.children_map:
                self.children_map[joint_info.parent] = []
            self.children_map[joint_info.parent].append(
                (joint_info.child, joint_info))

    def _create_link_entity(self, link_name: str, link_info: LinkInfo):
        """为连杆创建可视化实体"""
        # 生成网格
        if link_info.geometry_type == 'cylinder':
            vertices, indices = create_cylinder_mesh(
                link_info.geometry_params['radius'],
                link_info.geometry_params['length']
            )
        elif link_info.geometry_type == 'box':
            vertices, indices = create_box_mesh(
                link_info.geometry_params['size'])
        else:
            # 默认圆柱体
            vertices, indices = create_cylinder_mesh(0.02, 0.05)

        # 创建材质
        mat_json = mat.OpenPBRInterface(app._project)
        mat_json.set_specular_roughness(0.5)
        mat_json.set_weight_metallic(0.3)
        mat_json.set_base_albedo(link_info.material_color[:3])

        material = re.world.MaterialResource()
        material.load_from_json(mat_json.dump_to_json())
        del mat_json

        mat_vector = lc.capsule_vector()
        mat_vector.emplace_back(material._handle)

        # 创建实体
        entity = self.scene.add_entity()
        entity.set_name(f"link_{link_name}")

        transform = re.world.TransformComponent(
            entity.add_component("TransformComponent")
        )
        render = re.world.RenderComponent(
            entity.add_component("RenderComponent")
        )

        # 创建网格资源
        mesh = re.world.MeshResource()
        submesh_offsets = np.array([0], dtype=np.uint32)
        mesh.create_empty(submesh_offsets, len(vertices),
                          len(indices), 0, False, False)

        # 填充顶点数据
        pos_buffer = np.ndarray(
            len(vertices) * 4,
            dtype=np.float32,
            buffer=mesh.data_buffer()
        )
        pos_buffer[:len(vertices) * 4] = vertices.flatten()

        # 填充索引数据
        idx_buffer = np.ndarray(
            len(indices) * 3,
            dtype=np.uint32,
            buffer=mesh.data_buffer(),
            offset=len(vertices) * 4 * 4  # 4 bytes per float
        )
        idx_buffer[:] = indices.flatten()

        mesh.install()
        render.update_object(mat_vector, mesh)

        self.link_entities[link_name] = entity
        self.link_transforms[link_name] = transform

    def _apply_joint_transforms(self):
        """应用关节变换到连杆"""
        # 找到根连杆（没有父关节的连杆）
        root_links = set(self.links.keys())
        for joint_info in self.joints.values():
            if joint_info.child in root_links:
                root_links.remove(joint_info.child)

        # 从根连杆开始递归设置变换
        for root in root_links:
            self._update_link_transform(root, lc.double3(0, 0, 0), np.eye(3))

    def _update_link_transform(self, link_name: str, parent_pos, parent_rot):
        """递归更新连杆变换"""
        print('ret')
        if link_name not in self.children_map:
            return

        for child_name, joint_info in self.children_map[link_name]:
            print('start')
            # 计算关节位置（相对于父连杆）
            print(joint_info.origin_xyz)
            joint_pos = np.array(joint_info.origin_xyz)

            # # 获取连杆变换组件
            # if child_name in self.link_transforms:
            #     transform = self.link_transforms[child_name]

            #     # 设置绝对位置
            #     abs_pos = parent_pos + lc.double3(*joint_pos)
            #     transform.set_pos(abs_pos, False)

            # 递归更新子连杆
            print(child_name)
            new_pos = lc.double3(parent_pos[0], parent_pos[1], parent_pos[2]) + lc.double3(
                joint_pos[0], joint_pos[1], joint_pos[2])
            self._update_link_transform(child_name,
                                        new_pos,
                                        parent_rot)
            print(1)

    def animate_joints(self, time: float):
        """
        动画关节

        Args:
            time: 当前时间（秒）
        """
        # 简单的正弦波动画，每个关节有不同的频率
        joint_angles = {
            'joint1': math.sin(time * 0.5) * 0.5,      # 基座旋转
            'joint2': math.sin(time * 0.7) * 0.8,      # 肩关节
            'joint3': math.sin(time * 0.9 + 1) * 0.6,  # 肘关节
            'joint4': math.sin(time * 1.1) * 0.5,      # 手腕旋转
            'joint5': math.sin(time * 1.3 + 2) * 0.4,  # 手腕俯仰
            'joint6': math.sin(time * 1.5) * 0.3,      # 末端旋转
        }

        # 应用关节角度
        for joint_name, angle in joint_angles.items():
            if joint_name in self.joints:
                self._set_joint_angle(joint_name, angle)

    def _set_joint_angle(self, joint_name: str, angle: float):
        """
        设置关节角度

        Args:
            joint_name: 关节名称
            angle: 角度（弧度）
        """
        if joint_name not in self.joints:
            return

        joint = self.joints[joint_name]
        child_link = joint.child

        if child_link not in self.link_transforms:
            return

        transform = self.link_transforms[child_link]

        # 计算旋转四元数
        axis = np.array(joint.axis)
        axis = axis / np.linalg.norm(axis)  # 归一化

        # 轴角转四元数
        half_angle = angle / 2
        sin_half = math.sin(half_angle)
        cos_half = math.cos(half_angle)

        qx = axis[0] * sin_half
        qy = axis[1] * sin_half
        qz = axis[2] * sin_half
        qw = cos_half

        transform.set_rotation(lc.float4(qx, qy, qz, qw), False)



# ============================================================
# 主程序
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="URDF Robot Arm Visualization",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s -p ./my_project                    # 运行交互式可视化
  %(prog)s -p ./my_project -b vk              # 使用 Vulkan 后端
  %(prog)s -p ./my_project --output           # 导出模式（无 GUI）
  %(prog)s -p ./my_project -r                 # 运行渲染案例
  %(prog)s -p ./my_project -r --output-image ./output.png
        """
    )
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="图形后端 API 类型, dx/vk (默认: dx)",
    )
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        help="rbc 项目路径，包含 rbc_project.json 的目录 (默认: 自动查找)",
        default=None,
    )
    parser.add_argument(
        "-o",
        "--output",
        action="store_true",
        help="导出模式（无 GUI）"
    )
    parser.add_argument(
        "--output-image",
        type=str,
        help="渲染案例的输出图像路径 (默认: <project>/urdf_render.png)",
        default=None
    )
    args = parser.parse_args()

    # 处理项目路径
    if args.project:
        project_path = Path(args.project)
    else:
        print('Bad project path.')
        sys.exit(1)

    # 初始化 RoboCute
    global app
    app = rbc.app.App()
    app.init(
        backend_name=args.backend,
        project_path=project_path,
        require_render=True
    )

    if not app.ctx:
        print("上下文初始化失败！")
        return

    # 初始化显示
    app.init_display(1920, 1080, "URDF 机械臂可视化")
    if not app.display_cam:
        print("显示初始化失败！")
        return

    # 设置相机位置（根据缩放因子调整距离）
    transform = app.get_display_transform()
    if transform:
        # 根据缩放因子调整相机距离，确保能看到完整的机械臂
        cam_distance = 0.8
        cam_height = 0.3
        transform.set_pos(lc.double3(cam_distance, cam_height, -cam_distance * 1.6), False)
        transform.set_rotation(lc.float4(0, 0, 0, 1), False)

    app.ctx.enable_camera_control()

    if not app.scene:
        print("场景无效！")
        return

    print("正在创建 URDF 机械臂场景...")

    # 创建 URDF 字符串
    print(f"使用缩放因子: {ROBOT_SCALE}x")
    urdf_xml = create_mycobot_280_urdf(ROBOT_SCALE)
    print('done')

    # 保存 URDF 文件（可选）
    # urdf_path = Path(__file__).parent / "mycobot_280.urdf"
    # with open(urdf_path, 'w', encoding='utf-8') as f:
    #     f.write(urdf_xml)
    # print(f"URDF 已保存到: {urdf_path}")

    # 创建机械臂可视化器
    visualizer = RobotArmVisualizer(app.scene)
    visualizer.build_from_urdf(urdf_xml)

    print(
        f"机械臂已创建，包含 {len(visualizer.links)} 个连杆和 {len(visualizer.joints)} 个关节")

    # 打印关节信息
    print("\n关节列表:")
    for name, joint in visualizer.joints.items():
        print(f"  - {name}: {joint.type}, 父: {joint.parent}, 子: {joint.child}")

    # 主循环
    last_time = time.time()
    frame_index = 0
    tick_stage = re.world.TickStage.PathTracingPreview

    print("\n控制说明:")
    print("  - 鼠标控制相机")
    print("  - 机械臂关节自动动画")
    print("  - 关闭窗口退出")

    try:
        while not app.ctx.should_close():
            cur_time = time.time()
            delta_time = cur_time - last_time
            last_time = cur_time

            # 更新关节动画
            visualizer.animate_joints(cur_time)

            app.display_cam.set_frame_index(frame_index)

            app.ctx.tick(delta_time, tick_stage, True)
            frame_index = 0

    except KeyboardInterrupt:
        print("\n用户中断")

    print("程序退出")
    lc.synchronize()

    # 清理
    app = None


if __name__ == "__main__":
    main()
