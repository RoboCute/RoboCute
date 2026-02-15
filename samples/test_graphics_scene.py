"""
图形场景测试脚本
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
    - 加载指定project和场景
    - 运行渲染循环, 支持相机控制和实时预览
"""

import robocute as rbc
import argparse
from pathlib import Path


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

    print(args.project)

    project_path = Path(args.project)

    app = rbc.app.App()
    app.init(project_path)
    app.init_display()

    app.run()


if __name__ == "__main__":
    main()
