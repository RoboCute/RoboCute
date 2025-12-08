## 环境

1. 能够访问 github 的网络环境
2. 安装 [xmake](https://github.com/xmake-io/xmake/releases) 
3. 安装 [llvm 编译工具链](https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/LLVM-19.1.7-win64.exe) 安装完成后保证 bin 目录在PATH中，方便构建系统寻找
4. 安装 [Cuda](https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64)
5. 安装 [Qt 6.8+](https://www.qt.io/) 用来编译GUI程序

## 配置项目

### 一键初始化：

- `setup.cmd`
- `setup.sh`

### Python环境配置

- 安装uv
- `uv sync --extra cu128/cu130`同步所有包
- (Optional) Pylance Select Interpreter `.venv/Scripts/python.exe`

### RBC环境启动

1. 安装环境 `uv run prepare`
2. 从src/meta中生成接口代码：`uv run gen`
3. 配置 `xmake f -m release -c`
4. 编译 `xmake`

### 发布运行

1. 运行安装脚本 `xmake l /xmake/install.lua`
2. 尝试看绑定是否正确`uv run samples/bind_struct.py`
3. 运行测试用例 `uv run pytest`
4. 尝试QtNode连连看`xmake run calculator`

运行`xmake f -p windows -m release -c`时如果xmake没有使用最新版本的msvc，可以通过`--vs_toolset=`指定
