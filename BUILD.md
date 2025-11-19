## 环境

1. 能够访问 github 的网络环境
2. 安装 [xmake](https://github.com/xmake-io/xmake/releases) 
3. 安装 [llvm 编译工具链](https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/LLVM-19.1.7-win64.exe) 安装完成后保证 bin 目录在PATH中，方便构建系统寻找
4. 安装 [Cuda](https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64)

## 配置项目

### Python环境配置

- 安装uv
- `uv sync`同步所有包
- (Optional) Pylance Select Interpreter `.venv/Scripts/python.exe`

### RBS环境启动

1. 安装环境 `uv run scripts/setup.py`
2. 生成代码 `uv run generate.py`
3. 配置 `xmake f -m release -c`
4. 编译 `xmake`

 运行`xmake f -p windows -m release -c`时如果xmake没有使用最新版本的msvc，可以通过`--vs_toolset=`指定
