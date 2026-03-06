"""
USD (Universal Scene Description) 测试案例

展示如何使用 usd-core 创建复杂场景，包括:
- 创建场景层级结构
- 添加多种几何体 (立方体、球体、圆柱体、圆锥体)
- 设置变换 (位置、旋转、缩放)
- 添加材质和颜色
- 保存为 USDA (文本) 和 USDC (二进制) 格式
- 使用 RoboCute 渲染场景
"""

from pxr import Usd, UsdGeom, Gf, Sdf
from typing import Optional
import os
import argparse
from pathlib import Path
import numpy as np
import math

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
from robocute.rbc_ext._C import lcapi_c as lcapi
import mat_builtin as mat


app: rbc.app.App = None


def create_transform_attrs(
    prim: UsdGeom.Boundable,
    translation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    rotation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    scale: Gf.Vec3d = Gf.Vec3d(1, 1, 1),
) -> None:
    """
    为 prim 设置变换属性 (平移、旋转、缩放)

    Args:
        prim: USD 几何体 prim
        translation: 平移向量
        rotation: 旋转角度 (Euler angles, degrees)
        scale: 缩放向量
    """
    # 获取或创建变换属性
    xform = UsdGeom.Xformable(prim.GetPrim())

    # 清除现有的变换操作
    xform.ClearXformOpOrder()

    # 添加变换操作
    translate_op = xform.AddTranslateOp()
    translate_op.Set(translation)

    rotate_op = xform.AddRotateXYZOp()
    rotate_op.Set(rotation)

    scale_op = xform.AddScaleOp()
    scale_op.Set(scale)


def set_display_color(prim: UsdGeom.Boundable, color: Gf.Vec3f) -> None:
    """
    设置几何体的显示颜色

    Args:
        prim: USD 几何体 prim
        color: RGB 颜色值 (0-1 范围)
    """
    gprim = UsdGeom.Gprim(prim.GetPrim())
    color_attr = gprim.GetDisplayColorAttr()
    color_attr.Set([color])


def create_cube(
    stage: Usd.Stage,
    path: str,
    size: float = 1.0,
    translation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    color: Optional[Gf.Vec3f] = None,
) -> UsdGeom.Cube:
    """
    创建立方体

    Args:
        stage: USD stage
        path: prim 路径
        size: 立方体大小
        translation: 位置
        color: 显示颜色

    Returns:
        创建的 Cube prim
    """
    cube = UsdGeom.Cube.Define(stage, path)
    cube.GetSizeAttr().Set(size)
    create_transform_attrs(cube, translation=translation)

    if color:
        set_display_color(cube, color)

    return cube


def create_sphere(
    stage: Usd.Stage,
    path: str,
    radius: float = 1.0,
    translation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    color: Optional[Gf.Vec3f] = None,
) -> UsdGeom.Sphere:
    """
    创建球体

    Args:
        stage: USD stage
        path: prim 路径
        radius: 球体半径
        translation: 位置
        color: 显示颜色

    Returns:
        创建的 Sphere prim
    """
    sphere = UsdGeom.Sphere.Define(stage, path)
    sphere.GetRadiusAttr().Set(radius)
    create_transform_attrs(sphere, translation=translation)

    if color:
        set_display_color(sphere, color)

    return sphere


def create_cylinder(
    stage: Usd.Stage,
    path: str,
    radius: float = 0.5,
    height: float = 2.0,
    translation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    color: Optional[Gf.Vec3f] = None,
) -> UsdGeom.Cylinder:
    """
    创建圆柱体

    Args:
        stage: USD stage
        path: prim 路径
        radius: 圆柱体半径
        height: 圆柱体高度
        translation: 位置
        color: 显示颜色

    Returns:
        创建的 Cylinder prim
    """
    cylinder = UsdGeom.Cylinder.Define(stage, path)
    cylinder.GetRadiusAttr().Set(radius)
    cylinder.GetHeightAttr().Set(height)
    create_transform_attrs(cylinder, translation=translation)

    if color:
        set_display_color(cylinder, color)

    return cylinder


def create_cone(
    stage: Usd.Stage,
    path: str,
    radius: float = 0.5,
    height: float = 2.0,
    translation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    color: Optional[Gf.Vec3f] = None,
) -> UsdGeom.Cone:
    """
    创建圆锥体

    Args:
        stage: USD stage
        path: prim 路径
        radius: 圆锥体底部半径
        height: 圆锥体高度
        translation: 位置
        color: 显示颜色

    Returns:
        创建的 Cone prim
    """
    cone = UsdGeom.Cone.Define(stage, path)
    cone.GetRadiusAttr().Set(radius)
    cone.GetHeightAttr().Set(height)
    create_transform_attrs(cone, translation=translation)

    if color:
        set_display_color(cone, color)

    return cone


def create_scene(stage: Usd.Stage) -> None:
    """
    创建一个示例场景，包含多种几何体

    Args:
        stage: USD stage
    """
    # 创建根节点
    world = UsdGeom.Xform.Define(stage, "/World")

    # 定义颜色 (RGB, 0-1 范围)
    red = Gf.Vec3f(1.0, 0.0, 0.0)
    green = Gf.Vec3f(0.0, 1.0, 0.0)
    blue = Gf.Vec3f(0.0, 0.0, 1.0)
    yellow = Gf.Vec3f(1.0, 1.0, 0.0)
    cyan = Gf.Vec3f(0.0, 1.0, 1.0)
    magenta = Gf.Vec3f(1.0, 0.0, 1.0)

    # 创建立方体
    create_cube(
        stage,
        "/World/Cube",
        size=1.0,
        translation=Gf.Vec3d(-2, 0.5, 0),
        color=red,
    )

    # 创建球体
    create_sphere(
        stage,
        "/World/Sphere",
        radius=0.8,
        translation=Gf.Vec3d(0, 0.8, 0),
        color=green,
    )

    # 创建圆柱体
    create_cylinder(
        stage,
        "/World/Cylinder",
        radius=0.5,
        height=2.0,
        translation=Gf.Vec3d(2, 1.0, 0),
        color=blue,
    )

    # 创建圆锥体
    create_cone(
        stage,
        "/World/Cone",
        radius=0.6,
        height=1.5,
        translation=Gf.Vec3d(0, 2.5, 2),
        color=yellow,
    )

    # 创建一个旋转的立方体
    cube_rotated = create_cube(
        stage,
        "/World/CubeRotated",
        size=0.8,
        translation=Gf.Vec3d(-2, 2, 2),
        color=cyan,
    )
    create_transform_attrs(
        cube_rotated,
        translation=Gf.Vec3d(-2, 2, 2),
        rotation=Gf.Vec3d(45, 45, 0),
        scale=Gf.Vec3d(1, 1, 1),
    )

    # 创建缩放的球体
    sphere_scaled = create_sphere(
        stage,
        "/World/SphereScaled",
        radius=1.0,
        translation=Gf.Vec3d(2, 2, 2),
        color=magenta,
    )
    create_transform_attrs(
        sphere_scaled,
        translation=Gf.Vec3d(2, 2, 2),
        scale=Gf.Vec3d(1.5, 0.5, 1),
    )

    # 添加场景元数据
    root_layer = stage.GetRootLayer()
    root_layer.documentation = "USD Test Scene - Generated by test_usd.py"

    # 设置单位
    UsdGeom.SetStageMetersPerUnit(stage, 1.0)
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)


def print_stage_info(stage: Usd.Stage) -> None:
    """
    打印 Stage 信息

    Args:
        stage: USD stage
    """
    print("=" * 50)
    print("Stage Information:")
    print("=" * 50)
    print(f"Root Layer: {stage.GetRootLayer().identifier}")
    print(f"Default Prim: {stage.GetDefaultPrim().GetName() if stage.GetDefaultPrim() else 'None'}")
    print(f"Meters Per Unit: {UsdGeom.GetStageMetersPerUnit(stage)}")
    print(f"Up Axis: {UsdGeom.GetStageUpAxis(stage)}")
    print()

    print("Scene Hierarchy:")
    print("-" * 50)

    def traverse_prim(prim: Usd.Prim, indent: int = 0) -> None:
        prefix = "  " * indent
        print(f"{prefix}{prim.GetName()} ({prim.GetTypeName()})")
        for child in prim.GetChildren():
            traverse_prim(child, indent + 1)

    for prim in stage.GetPseudoRoot().GetChildren():
        traverse_prim(prim)

    print("=" * 50)


def gf_vec3d_to_lc_double3(vec: Gf.Vec3d) -> lc.double3:
    """将 Gf.Vec3d 转换为 lc.double3"""
    return lc.double3(vec[0], vec[1], vec[2])


def gf_vec3f_to_lc_float3(vec: Gf.Vec3f) -> lc.float3:
    """将 Gf.Vec3f 转换为 lc.float3"""
    return lc.float3(vec[0], vec[1], vec[2])


def gf_vec3d_to_lc_float4_rotation(rotation: Gf.Vec3d) -> lc.float4:
    """将 Euler 角度 (度) 转换为四元数 (lc.float4)"""
    # 将角度转换为弧度
    rx = math.radians(rotation[0])
    ry = math.radians(rotation[1])
    rz = math.radians(rotation[2])
    
    # 计算半角的 sin 和 cos
    cx = math.cos(rx * 0.5)
    sx = math.sin(rx * 0.5)
    cy = math.cos(ry * 0.5)
    sy = math.sin(ry * 0.5)
    cz = math.cos(rz * 0.5)
    sz = math.sin(rz * 0.5)
    
    # 计算四元数 (XYZ 旋转顺序)
    qw = cx * cy * cz + sx * sy * sz
    qx = sx * cy * cz - cx * sy * sz
    qy = cx * sy * cz + sx * cy * sz
    qz = cx * cy * sz - sx * sy * cz
    
    return lc.float4(qx, qy, qz, qw)


def create_cube_mesh_entity(
    scene: re.world.Scene,
    name: str,
    size: float,
    translation: Gf.Vec3d,
    rotation: Gf.Vec3d = Gf.Vec3d(0, 0, 0),
    scale: Gf.Vec3d = Gf.Vec3d(1, 1, 1),
    color: Optional[Gf.Vec3f] = None,
    tex: Optional[re.world.TextureResource] = None,
) -> re.world.Entity:
    """
    在 RoboCute 场景中创建立方体网格实体

    Args:
        scene: RoboCute 场景
        name: 实体名称
        size: 立方体大小
        translation: 位置
        rotation: 旋转角度 (Euler angles, degrees)
        scale: 缩放
        color: 颜色
        tex: 纹理资源

    Returns:
        创建的实体
    """
    mat_res = re.world.MaterialResource()
    mat_json = mat.OpenPBRInterface(app._project)
    mat_json.set_specular_roughness(0.5)
    mat_json.set_weight_metallic(0.3)
    
    if color:
        mat_json.set_base_albedo((color[0], color[1], color[2]))
    else:
        mat_json.set_base_albedo((0.8, 0.8, 0.8))
    
    if tex:
        mat_json.set_base_albedo_tex(tex)
    
    mat_res.load_from_json(mat_json.dump_to_json())
    del mat_json
    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(mat_res._handle)
    
    entity = scene.add_entity()
    entity.set_name(name)
    entity = scene.get_entity_by_name(name)
    assert entity
    trans = re.world.TransformComponent(entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))
    
    # 设置位置和旋转
    pos = gf_vec3d_to_lc_double3(translation)
    trans.set_pos(pos, False)
    
    if rotation != Gf.Vec3d(0, 0, 0):
        rot_quat = gf_vec3d_to_lc_float4_rotation(rotation)
        trans.set_rotation(rot_quat, False)
    
    # 创建立方体网格
    cube_mesh = create_cube_mesh(size, scale)
    render.update_object(mat_vector, cube_mesh)
    
    return entity


def create_cube_mesh(size: float, scale: Gf.Vec3d = Gf.Vec3d(1, 1, 1)) -> re.world.MeshResource:
    """
    创建立方体网格资源

    Args:
        size: 立方体大小
        scale: 缩放

    Returns:
        网格资源
    """
    # 立方体: 8 个顶点, 12 个三角形
    vertex_count = 8
    triangle_count = 12
    
    mesh = re.world.MeshResource()
    submesh_offsets = np.array([0], dtype=np.uint32)
    mesh.create_empty(submesh_offsets, vertex_count, triangle_count, 1, False, False)
    
    # 数据布局: positions + UVs + indices
    mesh_array = np.ndarray(
        vertex_count * 4 + vertex_count * 2 + triangle_count * 3,
        dtype=np.float32,
        buffer=mesh.data_buffer(),
    )
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
    
    half_size = size * 0.5
    sx, sy, sz = scale[0], scale[1], scale[2]
    
    # 顶点位置 (应用缩放)
    positions = [
        (-half_size * sx, -half_size * sy, -half_size * sz),  # 0: 左下后
        (-half_size * sx, -half_size * sy, half_size * sz),   # 1: 左下前
        (half_size * sx, -half_size * sy, -half_size * sz),   # 2: 右下后
        (half_size * sx, -half_size * sy, half_size * sz),    # 3: 右下前
        (-half_size * sx, half_size * sy, -half_size * sz),   # 4: 左上后
        (-half_size * sx, half_size * sy, half_size * sz),    # 5: 左上前
        (half_size * sx, half_size * sy, -half_size * sz),    # 6: 右上后
        (half_size * sx, half_size * sy, half_size * sz),     # 7: 右上前
    ]
    
    # UV 坐标
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
    
    # 三角形索引
    indices = [
        # 底面
        0, 1, 2, 1, 3, 2,
        # 顶面
        4, 6, 5, 5, 6, 7,
        # 左面
        0, 4, 1, 1, 4, 5,
        # 右面
        2, 3, 6, 3, 7, 6,
        # 后面
        0, 2, 4, 2, 6, 4,
        # 前面
        1, 5, 3, 3, 5, 7,
    ]
    # 填充数据
    idx = 0
    for pos in positions:
        vertex_arr[idx] = pos[0]
        vertex_arr[idx + 1] = pos[1]
        vertex_arr[idx + 2] = pos[2]
        vertex_arr[idx + 3] = 1.0
        idx += 4
    idx = 0
    for uv in uvs:
        uv_arr[idx] = uv[0]
        uv_arr[idx + 1] = uv[1]
        idx += 2
    idx = 0
    for i in indices:
        indices_arr[idx] = i
        idx += 1
    
    mesh.install()
    return mesh


def render_usd_scene(
    stage: Usd.Stage,
    project_path: Path,
    backend_name: str = "dx",
) -> None:
    """
    使用 RoboCute 渲染 USD 场景

    Args:
        stage: USD stage
        project_path: 项目路径
        backend_name: 后端名称
    """
    global app
    app = rbc.app.App()
    app.init(project_path=project_path, backend_name=backend_name)
    
    # 导入纹理
    tex = app._project.import_texture('test_grid.png', 4, True)
    print(f"Texture size: {tex.size()}")
    
    if not app.ctx:
        print("Context not Valid!")
        return
    
    resolution = lc.uint2(1920, 1080)
    app.init_display(resolution.x, resolution.y)
    
    if not app.display_cam:
        print("Display not Valid!")
        return
    
    # 设置相机位置
    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 2, -6), False)
        transform.set_rotation(lc.float4(0, -1, 0, 0), False)
    
    app.ctx.enable_camera_control()
    
    if not app.scene:
        print("Scene not Valid!")
        return
    
    # 遍历 USD 场景并创建渲染实体
    world_prim = stage.GetPrimAtPath("/World")
    print('Start create world primitive')
    if world_prim:
        for child in world_prim.GetChildren():
            prim_type = child.GetTypeName()
            prim_name = child.GetName()
            
            # 获取变换
            xform = UsdGeom.Xformable(child)
            transform_ops = xform.GetOrderedXformOps()
            
            translation = Gf.Vec3d(0, 0, 0)
            rotation = Gf.Vec3d(0, 0, 0)
            scale = Gf.Vec3d(1, 1, 1)
            
            for op in transform_ops:
                if op.GetOpType() == UsdGeom.XformOp.TypeTranslate:
                    translation = op.Get()
                elif op.GetOpType() == UsdGeom.XformOp.TypeRotateXYZ:
                    rotation = op.Get()
                elif op.GetOpType() == UsdGeom.XformOp.TypeScale:
                    scale = op.Get()
            
            # 获取颜色
            color = None
            if prim_type in ["Cube", "Sphere", "Cylinder", "Cone"]:
                gprim = UsdGeom.Gprim(child)
                color_attr = gprim.GetDisplayColorAttr()
                if color_attr.HasValue():
                    colors = color_attr.Get()
                    if colors:
                        color = colors[0]
            
            # 根据类型创建实体
            if prim_type == "Cube":
                cube_geom = UsdGeom.Cube(child)
                size = cube_geom.GetSizeAttr().Get()
                if size is None:
                    size = 1.0
                create_cube_mesh_entity(
                    app.scene, str(prim_name), size,
                    translation, rotation, scale, color, tex
                )
                print(f"Created cube: {prim_name} at {translation}")
            elif prim_type == "Sphere":
                # 使用立方体近似球体 (或者可以创建更复杂的球体网格)
                sphere_geom = UsdGeom.Sphere(child)
                radius = sphere_geom.GetRadiusAttr().Get()
                if radius is None:
                    radius = 1.0
                # 使用立方体作为球体的近似，缩放为球体大小
                sphere_scale = Gf.Vec3d(
                    scale[0] * radius * 2,
                    scale[1] * radius * 2,
                    scale[2] * radius * 2
                )
                create_cube_mesh_entity(
                    app.scene, str(prim_name), 1.0,
                    translation, rotation, sphere_scale, color, tex
                )
                print(f"Created sphere: {prim_name} at {translation}")
            elif prim_type == "Cylinder":
                cylinder_geom = UsdGeom.Cylinder(child)
                radius = cylinder_geom.GetRadiusAttr().Get() or 0.5
                height = cylinder_geom.GetHeightAttr().Get() or 2.0
                cyl_scale = Gf.Vec3d(
                    scale[0] * radius * 2,
                    scale[1] * height,
                    scale[2] * radius * 2
                )
                create_cube_mesh_entity(
                    app.scene, str(prim_name), 1.0,
                    translation, rotation, cyl_scale, color, tex
                )
                print(f"Created cylinder: {prim_name} at {translation}")
            elif prim_type == "Cone":
                cone_geom = UsdGeom.Cone(child)
                radius = cone_geom.GetRadiusAttr().Get() or 0.5
                height = cone_geom.GetHeightAttr().Get() or 2.0
                cone_scale = Gf.Vec3d(
                    scale[0] * radius * 2,
                    scale[1] * height,
                    scale[2] * radius * 2
                )
                create_cube_mesh_entity(
                    app.scene, str(prim_name), 1.0,
                    translation, rotation, cone_scale, color, tex
                )
                print(f"Created cone: {prim_name} at {translation}")
    
    print("Starting render loop...")
    print("Press ESC or close window to exit")
    
    import time
    frame_index = 0
    last_time = time.time()
    tick_stage = re.world.TickStage.PathTracingPreview
    
    # 渲染循环
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


def main() -> None:
    """主函数 - 创建并保存 USD 场景，可选渲染"""
    parser = argparse.ArgumentParser(description="USD Test Scene")
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
    )
    parser.add_argument(
        "-r",
        "--render",
        action="store_true",
        help="Render the scene using RoboCute",
    )
    args = parser.parse_args()
    
    # 获取输出目录
    output_dir = os.path.join(os.path.dirname(__file__), "output")
    os.makedirs(output_dir, exist_ok=True)

    # 文件路径
    usda_path = os.path.join(output_dir, "test_scene.usda")
    usdc_path = os.path.join(output_dir, "test_scene.usdc")

    print("Creating USD Test Scene...")
    print()

    # 创建新的 Stage (文本格式)
    stage = Usd.Stage.CreateNew(usda_path)

    # 设置默认 prim
    world = UsdGeom.Xform.Define(stage, "/World")
    stage.SetDefaultPrim(world.GetPrim())

    # 创建场景内容
    create_scene(stage)

    # 打印 Stage 信息
    print_stage_info(stage)

    # 保存为 USDA (文本格式)
    stage.GetRootLayer().Save()
    print(f"Saved to: {usda_path}")

    # 导出为 USDC (二进制格式)
    stage.Export(usdc_path)
    print(f"Exported to: {usdc_path}")

    print()
    print("USD Test Scene created successfully!")

    # 如果指定了渲染选项
    if args.render:
        if not args.project:
            print("\nError: --project is required for rendering")
            print("Usage: python test_usd.py --render --project <path_to_project>")
            return
        
        print("\nStarting RoboCute rendering...")
        project_path = Path(args.project)
        render_usd_scene(stage, project_path, args.backend)
    else:
        # 读取并显示 USDA 文件内容 (前 50 行)
        print()
        print("=" * 50)
        print("USDA File Content (first 50 lines):")
        print("=" * 50)
        with open(usda_path, "r") as f:
            lines = f.readlines()[:50]
            for line in lines:
                print(line.rstrip())
        print("=" * 50)
        print("\nUse --render flag to visualize the scene")


if __name__ == "__main__":
    main()
