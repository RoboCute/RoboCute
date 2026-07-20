"""
GLTF Skeleton Animation Resource Inspector

从GLTF文件中提取骨骼动画相关信息并打印的示例。
展示如何从Python侧读取和分析骨骼动画资源。

Usage:
    cd <project_root>
    uv run python -m samples.gltf_anim_inspector -p <project_path> -g <gltf_file>

Example:
    uv run python -m samples.gltf_anim_inspector \
        -p d:/ws/repos/RoboCute-repo/rbc-project-anim \
        -g assets/anim_test/test_anim.gltf
"""

import os
import sys
import argparse
from pathlib import Path
from typing import Optional, List, Dict, Any

# Add parent directory to path for samples module imports
script_dir = Path(__file__).parent
project_root = script_dir.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# Change to project root directory to ensure resources are loaded correctly
os.chdir(project_root)

import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
import robocute as rbc


def print_section(title: str) -> None:
    """Print a formatted section header."""
    print("\n" + "=" * 70)
    print(f" {title}")
    print("=" * 70)


def print_subsection(title: str) -> None:
    """Print a formatted subsection header."""
    print(f"\n--- {title} ---")


def inspect_skeleton(skeleton: re.world.SkeletonResource) -> None:
    """
    检查并打印骨架信息，包括骨骼层级细节。

    展示如何访问骨架资源的详细信息，这对物理动画非常重要。
    """
    print_section("骨架 (Skeleton) 信息")

    # 打印骨架资源基本信息
    print(f"资源类型: SkeletonResource")
    print(f"资源有效: {skeleton is not None}")

    try:
        # 获取关节数量
        num_joints = skeleton.get_num_joints()
        num_soa_joints = skeleton.get_num_soa_joints()

        print(f"\n关节数量:")
        print(f"  普通关节数: {num_joints}")
        print(f"  SOA关节数: {num_soa_joints}")

        if num_joints == 0:
            print("\n警告: 骨架中没有关节")
            return

        # 获取关节名称
        print_subsection("关节名称 (Joint Names)")
        joint_names = skeleton.get_joint_names()
        print(f"  关节总数: {len(joint_names)}")

        for i, name in enumerate(joint_names[:20]):  # 只显示前20个
            print(f"    [{i:3d}] {name}")
        if len(joint_names) > 20:
            print(f"    ... 还有 {len(joint_names) - 20} 个关节")

        # 获取父索引
        print_subsection("关节层级 (Joint Hierarchy)")
        joint_parents = skeleton.get_joint_parents()
        print(f"  父索引列表: {list(joint_parents[:20])}")
        if len(joint_parents) > 20:
            print(f"    ... 还有 {len(joint_parents) - 20} 个")

        # 打印层级树
        print("\n  骨骼层级树:")
        printed = set()

        def print_joint_tree(joint_idx: int, indent: int = 0):
            if joint_idx in printed:
                return
            printed.add(joint_idx)

            prefix = "  " * indent + ("└─ " if indent > 0 else "")
            name = (
                joint_names[joint_idx]
                if joint_idx < len(joint_names)
                else f"Joint_{joint_idx}"
            )
            parent_idx = (
                joint_parents[joint_idx] if joint_idx < len(joint_parents) else -1
            )

            if parent_idx >= 0:
                parent_name = (
                    joint_names[parent_idx]
                    if parent_idx < len(joint_names)
                    else f"Joint_{parent_idx}"
                )
                print(
                    f"{prefix}[{joint_idx:3d}] {name} (parent: {parent_name}[{parent_idx}])"
                )
            else:
                print(f"{prefix}[{joint_idx:3d}] {name} (ROOT)")

            # 找到所有子关节
            for i, parent in enumerate(joint_parents):
                if parent == joint_idx:
                    print_joint_tree(i, indent + 1)

        # 从根关节开始打印（parent为-1或超出范围的）
        for i in range(min(num_joints, 50)):  # 限制最多50个关节
            parent = joint_parents[i] if i < len(joint_parents) else -1
            if parent < 0 or parent >= num_joints:
                print_joint_tree(i, 0)

        # 获取休息姿态
        print_subsection("休息姿态 (Rest Poses)")
        rest_poses = skeleton.get_joint_rest_poses()
        print(f"  休息姿态数量: {len(rest_poses)}")

        if len(rest_poses) > 0:
            print("\n  前3个关节的休息姿态矩阵:")
            for i in range(min(3, len(rest_poses))):
                name = joint_names[i] if i < len(joint_names) else f"Joint_{i}"
                matrix = rest_poses[i]
                print(f"    [{i}] {name}:")
                print(
                    f"        [{matrix[0][0]:.4f}, {matrix[0][1]:.4f}, {matrix[0][2]:.4f}, {matrix[0][3]:.4f}]"
                )
                print(
                    f"        [{matrix[1][0]:.4f}, {matrix[1][1]:.4f}, {matrix[1][2]:.4f}, {matrix[1][3]:.4f}]"
                )
                print(
                    f"        [{matrix[2][0]:.4f}, {matrix[2][1]:.4f}, {matrix[2][2]:.4f}, {matrix[2][3]:.4f}]"
                )
                print(
                    f"        [{matrix[3][0]:.4f}, {matrix[3][1]:.4f}, {matrix[3][2]:.4f}, {matrix[3][3]:.4f}]"
                )

        # 调用简要日志输出
        print("\n调用 log_brief() 输出骨架信息:")
        skeleton.log_brief()

    except Exception as e:
        print(f"获取骨架详情失败: {e}")
        import traceback

        traceback.print_exc()

    print("\n" + "-" * 70)
    print("✓ 骨骼层级信息导出完成")
    print("这些信息对物理动画非常重要:")
    print("  - 关节层级关系用于物理骨骼链计算")
    print("  - 休息姿态是物理模拟的初始状态")
    print("  - 父索引用于局部到世界空间的转换")


def inspect_anim_sequence(anim_seq: re.world.AnimSequenceResource) -> None:
    """
    检查并打印动画序列信息。

    展示如何访问动画序列资源的信息。
    """
    print_section("动画序列 (Animation Sequence) 信息")

    print(f"资源类型: AnimSequenceResource")
    print(f"资源有效: {anim_seq is not None}")

    try:
        # 获取AnimSequence对象
        seq = anim_seq.ref_seq()
        print(f"\nAnimSequence 对象: {seq}")

        # 获取轨道信息
        num_soa_tracks = seq.get_num_soa_tracks()
        num_tracks = seq.get_num_tracks()

        print(f"\n动画轨道信息:")
        print(f"  SOA Tracks数量: {num_soa_tracks}")
        print(f"  总Tracks数量: {num_tracks}")

        # 获取关联的骨架
        ref_skel = anim_seq.ref_skel()
        if ref_skel:
            print(f"\n关联骨架: {ref_skel}")
        else:
            print("\n关联骨架: None (未设置)")

        # 调用简要日志输出
        print("\n调用 log_brief() 输出动画信息:")
        anim_seq.log_brief()

    except Exception as e:
        print(f"获取动画序列信息失败: {e}")
        import traceback

        traceback.print_exc()

    print("\n" + "-" * 70)
    print("提示: 当前Python绑定提供的动画接口有限。")
    print("要获取完整的关键帧数据，需要扩展以下接口:")
    print("  - get_duration(): 获取动画时长")
    print("  - get_keyframe_count(track): 获取关键帧数量")
    print("  - get_keyframe_time(track, index): 获取关键帧时间")
    print("  - sample(time, skeleton): 在指定时间采样")


def inspect_skin(skin: re.world.SkinResource) -> None:
    """
    检查并打印皮肤/蒙皮信息。

    展示如何访问蒙皮资源的信息，包括骨骼绑定和逆绑定姿态。
    """
    print_section("蒙皮 (Skin) 信息")

    print(f"资源类型: SkinResource")
    print(f"资源有效: {skin is not None}")

    try:
        # 获取关联资源
        ref_skel = skin.ref_skel()
        ref_mesh = skin.ref_mesh()

        print_subsection("关联资源")
        print(f"  骨架: {ref_skel if ref_skel else 'None (未设置)'}")
        print(f"  网格: {ref_mesh if ref_mesh else 'None (未设置)'}")

        # 获取Joint重映射信息
        print_subsection("Joint重映射信息")
        joint_remaps = skin.JointRemaps()
        print(f"  Joint数量: {len(joint_remaps)}")

        if len(joint_remaps) > 0:
            print(f"\n  Joint名称列表 (前10个):")
            for i, name in enumerate(joint_remaps[:10]):
                print(f"    [{i}] {name}")
            if len(joint_remaps) > 10:
                print(f"    ... 还有 {len(joint_remaps) - 10} 个Joint")

        # 获取逆绑定姿态矩阵
        print_subsection("逆绑定姿态 (Inverse Bind Poses)")
        inverse_bind_poses = skin.InverseBindPoses()
        num_joints = len(joint_remaps)

        if len(inverse_bind_poses) > 0:
            print(f"  矩阵数量: {len(inverse_bind_poses)}")
            print(f"  预期数量: {num_joints} (每个Joint一个矩阵)")

            # 打印前3个Joint的逆绑定矩阵
            if num_joints > 0:
                print(f"\n  前3个Joint的逆绑定矩阵:")
                for i in range(min(3, num_joints)):
                    matrix = inverse_bind_poses[i]
                    print(
                        f"    [{i}] {joint_remaps[i] if i < len(joint_remaps) else 'Unknown'}:"
                    )
                    print(
                        f"        [{matrix[0][0]:.4f}, {matrix[0][1]:.4f}, {matrix[0][2]:.4f}, {matrix[0][3]:.4f}]"
                    )
                    print(
                        f"        [{matrix[1][0]:.4f}, {matrix[1][1]:.4f}, {matrix[1][2]:.4f}, {matrix[1][3]:.4f}]"
                    )
                    print(
                        f"        [{matrix[2][0]:.4f}, {matrix[2][1]:.4f}, {matrix[2][2]:.4f}, {matrix[2][3]:.4f}]"
                    )
                    print(
                        f"        [{matrix[3][0]:.4f}, {matrix[3][1]:.4f}, {matrix[3][2]:.4f}, {matrix[3][3]:.4f}]"
                    )
        else:
            print("  逆绑定姿态数据为空")

        # 获取Joint重映射LUT
        print_subsection("Joint重映射查找表 (LUT)")
        lut = skin.JointRemapsLUT()
        if len(lut) > 0:
            print(f"  LUT条目数: {len(lut)}")
            print(f"  前10个值: {list(lut[:10])}")
            if len(lut) > 10:
                print(f"  ... 还有 {len(lut) - 10} 个值")
        else:
            print("  LUT为空 (需要先调用 generate_LUT())")

        # 调用简要日志输出
        print("\n调用 log_brief() 输出皮肤信息:")
        skin.log_brief()

    except Exception as e:
        print(f"获取皮肤信息失败: {e}")
        import traceback

        traceback.print_exc()


def inspect_anim_graph(anim_graph: re.world.AnimGraphResource) -> None:
    """
    检查并打印动画图信息。
    """
    print_section("动画图 (Animation Graph) 信息")

    print(f"资源类型: AnimGraphResource")
    print(f"资源有效: {anim_graph is not None}")

    # 当前AnimGraphResource的接口非常有限
    print("\n" + "-" * 70)
    print("提示: 当前Python绑定中AnimGraphResource只提供基础功能。")
    print("要检查动画图结构，需要扩展以下接口:")
    print("  - get_node_count(): 获取节点数量")
    print("  - get_node_name(index): 获取节点名称")
    print("  - get_node_type(index): 获取节点类型")


def inspect_skel_mesh(skel_mesh: re.world.SkelMeshResource) -> None:
    """
    检查并打印骨骼网格信息。
    """
    print_section("骨骼网格 (SkelMesh) 信息")

    print(f"资源类型: SkelMeshResource")
    print(f"资源有效: {skel_mesh is not None}")

    try:
        # 获取关联资源
        skin = skel_mesh.ref_skin()
        skeleton = skel_mesh.ref_skeleton()
        anim_graph = skel_mesh.ref_anim_graph()

        print_subsection("关联资源")
        print(f"  皮肤: {skin if skin else 'None (未设置)'}")
        print(f"  骨架: {skeleton if skeleton else 'None (未设置)'}")
        print(f"  动画图: {anim_graph if anim_graph else 'None (未设置)'}")

    except Exception as e:
        print(f"获取骨骼网格信息失败: {e}")


def inspect_gltf_file(project: re.world.Project, gltf_path: str) -> None:
    """
    从GLTF文件中导入并检查所有骨骼动画相关资源。

    Args:
        project: 项目对象
        gltf_path: GLTF文件的相对路径 (相对于项目assets目录)
    """
    print(f"\n开始检查GLTF文件: {gltf_path}")
    print(f"项目路径: {project}")

    # 检查文件路径
    print(f"\n文件路径检查: {gltf_path}")

    # Step 1: 导入骨架
    print("\n" + ">" * 70)
    print("步骤 1/5: 导入骨架...")
    try:
        skeleton = project.import_skeleton(gltf_path)
        if skeleton:
            inspect_skeleton(skeleton)
        else:
            print("警告: 未能导入骨架 (可能GLTF中没有骨骼信息)")
    except Exception as e:
        print(f"导入骨架失败: {e}")
        skeleton = None

    # Step 2: 导入皮肤
    print("\n" + ">" * 70)
    print("步骤 2/5: 导入皮肤...")
    try:
        skin = project.import_skin(gltf_path)
        if skin:
            # 设置引用关系
            if skeleton:
                skin.ref_skel = skeleton
            inspect_skin(skin)
        else:
            print("警告: 未能导入皮肤 (可能GLTF中没有蒙皮信息)")
    except Exception as e:
        print(f"导入皮肤失败: {e}")
        skin = None

    # Step 3: 导入动画序列
    print("\n" + ">" * 70)
    print("步骤 3/5: 导入动画序列...")
    try:
        anim_seq = project.import_anim_sequence(gltf_path)
        if anim_seq:
            # 设置引用关系
            if skeleton:
                anim_seq.ref_skel = skeleton
            inspect_anim_sequence(anim_seq)
        else:
            print("警告: 未能导入动画序列 (可能GLTF中没有动画信息)")
    except Exception as e:
        print(f"导入动画序列失败: {e}")
        anim_seq = None

    # Step 4: 创建动画图
    print("\n" + ">" * 70)
    print("步骤 4/5: 创建动画图...")
    if anim_seq:
        try:
            anim_graph = re.world.AnimGraphResource()
            success = anim_graph.create_simple_anim_graph(anim_seq)
            if success:
                print("✓ 简单动画图创建成功")
                inspect_anim_graph(anim_graph)
            else:
                print("✗ 动画图创建失败")
        except Exception as e:
            print(f"创建动画图失败: {e}")
            anim_graph = None
    else:
        print("跳过: 没有动画序列，无法创建动画图")
        anim_graph = None

    # Step 5: 创建骨骼网格资源
    print("\n" + ">" * 70)
    print("步骤 5/5: 创建骨骼网格资源...")
    if skin and skeleton:
        try:
            skel_mesh = re.world.SkelMeshResource()
            skel_mesh.ref_skin = skin
            skel_mesh.ref_skeleton = skeleton
            if anim_graph:
                skel_mesh.ref_anim_graph = anim_graph
            inspect_skel_mesh(skel_mesh)
        except Exception as e:
            print(f"创建骨骼网格失败: {e}")
    else:
        print("跳过: 缺少皮肤或骨架资源")

    print("\n" + "=" * 70)
    print("GLTF资源检查完成!")
    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="GLTF Skeleton Animation Resource Inspector"
    )
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        required=True,
        help="RBC项目路径 (包含rbc_project.json的目录)",
    )
    parser.add_argument(
        "-g",
        "--gltf",
        type=str,
        required=True,
        help="GLTF文件路径 (相对于项目assets目录的相对路径，如: assets/anim_test/test_anim.gltf)",
    )
    args = parser.parse_args()

    project_path = Path(args.project)
    gltf_rel_path = args.gltf

    if not project_path.exists():
        print(f"错误: 项目路径不存在: {project_path}")
        return 1

    # 初始化RBC上下文 (最小化初始化，不需要渲染)
    print("初始化RBC环境...")
    try:
        # 初始化world
        world_path = project_path / "library"
        re.world.RBCContext.init_world(str(world_path / "meta.json"), str(world_path))

        # 创建项目对象
        project = re.world.Project()
        assets_dir = project_path / "assets"
        project.init(str(project_path))
        project.scan_project()

        print("✓ 初始化完成\n")

    except Exception as e:
        print(f"初始化失败: {e}")
        import traceback

        traceback.print_exc()
        return 1

    # 检查GLTF文件
    gltf_full_path = assets_dir / gltf_rel_path
    if not gltf_full_path.exists():
        print(f"错误: GLTF文件不存在: {gltf_full_path}")
        print(f"请确保路径是相对于 {assets_dir} 的相对路径")
        return 1

    # 执行资源检查
    try:
        inspect_gltf_file(project, gltf_rel_path)
    except Exception as e:
        print(f"检查过程中出错: {e}")
        import traceback

        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
