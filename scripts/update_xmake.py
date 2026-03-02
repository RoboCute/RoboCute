import shutil
import subprocess
import sys
from pathlib import Path


def remove_xmake_folder() -> None:
    xmakeDir = Path(__file__).parent.parent / Path(".xmake")
    if xmakeDir.exists() and xmakeDir.is_dir():
        print(f"正在删除文件夹: {xmakeDir}")
        shutil.rmtree(xmakeDir)
        print("删除完成")
    else:
        print(".xmake 文件夹不存在，跳过删除")


def update_xmake() -> int:
    print("正在执行: xmake update dev")
    result = subprocess.run(["xmake", "update", "dev"], shell=False)
    return result.returncode


def main() -> int:
    remove_xmake_folder()
    returnCode = update_xmake()
    
    if returnCode == 0:
        print("\nxmake 更新成功！")
    else:
        print(f"\nxmake 更新失败，返回码: {returnCode}")
    
    return returnCode


if __name__ == "__main__":
    sys.exit(main())
