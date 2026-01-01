# Robocute 编译/构建文档

robocute是一个python-first，带GUI和cpp runtime的大型库，所以整体构建流程会比较复杂

整体架构可以参考[Architecture](design/Architecture.md)

## 准备环境

1. 能够访问 github 的网络环境
2. 安装 [xmake](https://github.com/xmake-io/xmake/releases) 我们的cpp工程由xmake组织
3. 安装 [llvm 编译工具链](https://github.com/llvm/llvm-project/releases)，选择对应的平台版本安装，安装完成后保证 bin 目录在PATH中，方便构建系统寻找，开发版本主要在Windows机器上，会保证保证clang-cl一直可以顺利编译
4. 安装[uv](https://docs.astral.sh/) 用于管理python环境，同时支持部分项目构建脚本
5. 安装 [Qt 6.8+](https://www.qt.io/) 用来编译GUI程序
6. 安装[7-zip](https://www.7-zip.org/) 用来解压和安装预构建资源

## 配置编译

Robocute的构建核心是python脚本，我们会使用uv来进行python的包同步，同时也会使用python脚本来拉取C++的第三方源码库和必要的预构建资源。之后我们可以选择cmake和xmake两种方式来编译C++代码库，最终使用统一的python脚本完成最后的app安装，打包和分发。

### Python环境配置

Python在robocute中扮演双重角色：首先robocute最终会形成一个python package发布，相关的包和测试由python开发实现，另一方面python也扮演了一部分的构建脚本功能。

- 安装uv
- `uv sync`同步所有包
  - 如果希望增加一些对开发有帮助的库，增加`--extra dev`
  - 如果希望支持torch相关AIGC功能，增加`--extra cu128/cu130`
  - 如果希望支持uipc物理模拟，增加`--extra uipc`，并参考
  - 如果希望支持Robotics相关，增加`--extra robotics`
- (Optional) Pylance Select Interpreter `.venv/Scripts/python.exe`

### RBC环境启动

1. `uv run prepare`: 安装环境 下载cpp依赖的第三方库
2. `uv run gen`: 从`src/rbc_meta`中生成接口代码，代码生成可以保证很多需要重复定义的对象只需要一次代码编写，没有代码生成无法顺利编译cpp

### C++安装

xmake版本：
1. `xmake f -m release -c` 配置编译目标
2. `xmake` 执行编译 
3. `xmake l /xmake/install.lua` 将cpp release结果复制安装到希望的py ext位置 

cmake版本

1. `mkdir build_cmake && cd build_cmake`：创建cmake构建目标文件夹
2. `cmake .. -DQt6_ROOT=D:/tools/Qt/6.9.3/msvc2022_64/lib/cmake/Qt6`：寻找Qt6中`Qt6Config.cmake`的文件目录并通过`Qt6_ROOT`变量设置给cmake
3. `cmake --build . --config Release`：构建项目
4. `uv run install`：后处理，将必须的shader、默认场景、渲染默认资源复制到target

## 测试用例

xmake:
- rbc-editor
  - 启动开发服务器`uv run main.py`
  - 启动editor `xmake run rbc-editor`
- Graphics特性测试`xmake run test_graphics_bin`
- Serde特性测试`xmake run test_serde`

cmake:
- 确认Qt6的`bin`文件夹存在于系统`PATH`变量中
- rbc-editor
  - 启动开发服务器`uv run main.py`
  - 启动editor `cd ./build_cmake/bin/Release/ && ./rbc_editor`
- Graphics特性测试`cd ./build_cmake/bin/Release/ && ./test_graphics_bin`


## IDE支持

Robocute支持使用IDE进行开发和调试

### Compile Commands 支持

方便clangd等工具进行代码提示和补全

xmake: `xmake project -k compile_commands`
cmake: msvc暂时不支持生成，其他平台可以-D配置宏生成

### Visual Studio 支持

xmake: `xmake project -k vsxmake2022` 在vsxmake文件夹生成Visual Studio 2022的sln文件
cmake: configure的时候自动生成sln文件

### QtCreator支持

- 打开QtCreator，直接打开根目录下的`CMakeLists.txt`，正常Configure和构建即可
- QtCreator使用Cmake构建后未必能正常安装资源，必要时需要手动执行安装，寻找build目录下的Qt Build目录，执行脚本
  - `uv run python cmake/rbc_install_resources_post_build.py <project_dir> <target_dir>`
  - 例如 `uv run python cmake/rbc_install_resources_post_build.py . .\build\Desktop_Qt_6_9_3_MSVC2022_64bit-Debug\bin`
  - 将所需要的资源复制到指定位置

### Server-Editor

- find the registered node
- connect and execute
- fetch the result from python side
- playback in editor


### Test Graphics

![test_graphics_bin](images/test_graphics_bin.png)

