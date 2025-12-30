# Robocute 编译/构建文档

robocute是一个python-first，带GUI和cpp runtime的大型库，所以整体构建流程会比较复杂

整体架构可以参考[[doc/design/Architecture.md]]

## 准备环境

1. 能够访问 github 的网络环境
2. 安装 [xmake](https://github.com/xmake-io/xmake/releases) 我们的cpp工程由xmake组织
3. 安装 [llvm 编译工具链](https://github.com/llvm/llvm-project/releases)，选择对应的平台版本安装，安装完成后保证 bin 目录在PATH中，方便构建系统寻找，开发版本主要在Windows机器上，会保证保证clang-cl一直可以顺利编译
4. 安装[uv](https://docs.astral.sh/) 用于管理python环境，同时支持部分项目构建脚本
5. 安装 [Qt 6.8+](https://www.qt.io/) 用来编译GUI程序

## 配置编译

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

1. 安装环境 `uv run prepare` 下载cpp依赖的第三方库
2. 从`src/rbc_meta`中生成接口代码：`uv run gen` 代码生成可以保证很多需要重复定义的对象只需要一次代码编写，没有代码生成无法顺利编译cpp
3. 配置 `xmake f -m release -c`
4. 编译 `xmake`
5. 将cpp release结果复制安装到希望的py ext位置 `xmake l /xmake/install.lua`

## 测试用例

- rbc-editor
  - 启动开发服务器`uv run main.py`
  - 启动editor `xmake run rbc-editor`
- Graphics特性测试`xmake run test_graphics_bin`
- Serde特性测试`xmake run test_serde`

### Server-Editor

- find the registered node
- connect and execute
- fetch the result from python side
- playback in editor


### Test Graphics

![test_graphics_bin](images/test_graphics_bin.png)

## 发布运行

暂时仍在早期开发过程中，待补充