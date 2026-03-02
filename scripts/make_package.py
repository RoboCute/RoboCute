#!/usr/bin/env python3
"""
删除 src/ 目录下所有的 __pycache__ 文件夹。
"""

import threading
import os
import shutil
from pathlib import Path


count = 0


def delete_pycache(root_dir: str | Path) -> int:
    global count
    """删除指定目录下所有的 __pycache__ 文件夹。

    Args:
        root_dir: 要搜索的根目录路径

    Returns:
        删除的文件夹数量
    """
    root_path = Path(root_dir)

    for pycache_dir in root_path.rglob("__pycache__"):
        if pycache_dir.is_dir():
            print(f"删除: {pycache_dir}")
            shutil.rmtree(pycache_dir)
            count += 1


def main() -> None:
    global count
    """主函数。"""
    proj_dir = Path(__file__).parent.parent
    src_dir = proj_dir / "src"

    if not src_dir.exists():
        print(f"错误: 目录 {src_dir} 不存在")
        return

    samples_dir = proj_dir / "samples"

    if not src_dir.exists():
        print(f"错误: 目录 {src_dir} 不存在")
        return

    print(f"正在清理 {src_dir} 下的 __pycache__ 文件夹...")
    src_thread = threading.Thread(target=delete_pycache, args=(src_dir, ))
    src_thread.start()
    samples_thread = threading.Thread(
        target=delete_pycache, args=(samples_dir, ))
    samples_thread.start()

    src_thread.join()
    samples_thread.join()

    print(f"完成！共删除 {count} 个 __pycache__ 文件夹")
    out_dir = proj_dir / 'build' / 'out'

    # 创建 out_dir 并复制指定目录
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 复制 samples_dir 到 out_dir
    shutil.copytree(samples_dir, out_dir / 'samples')
    print(f"已复制 {samples_dir} 到 {out_dir / 'samples'}")

    # 复制 src_dir/"robocute" 到 out_dir
    robocute_dir = src_dir / "robocute"
    if robocute_dir.exists():
        shutil.copytree(robocute_dir, out_dir / 'src/robocute')
        print(f"已复制 {robocute_dir} 到 {out_dir / 'src/robocute'}")
    else:
        print(f"警告: 目录 {robocute_dir} 不存在，跳过复制")
    package_toml_dir = proj_dir / 'pyproject.toml'
    shutil.copy2(
        package_toml_dir,
        out_dir / 'pyproject.toml'
    )
    print(f"已复制 {package_toml_dir} 到 {out_dir / 'pyproject.toml'}")


if __name__ == "__main__":
    main()
