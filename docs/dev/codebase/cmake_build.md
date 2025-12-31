# CMake Build System

## 概述

RoboCute 项目现在支持使用 CMake 进行构建，与现有的 xmake 构建系统并行工作。CMake 构建系统完全实现了 xmake 中的 interface/impl 分离模式，并保持了与 Python 代码生成流程的兼容性。

## 构建流程

### 前置条件

1. **Python 代码生成**: 在运行 CMake 配置之前，需要先执行 Python 代码生成步骤：
   ```bash
   python src/scripts/main.py generate
   ```

2. **第三方库下载**: 第三方库的下载由 Python 脚本处理：
   ```bash
   python src/scripts/main.py prepare
   ```

3. **CMake 版本**: 需要 CMake 3.20 或更高版本

### 基本构建步骤

```bash
# 1. 创建构建目录
mkdir build_cmake
cd build_cmake

# 2. 配置 CMake
cmake ..

# 3. 构建项目
cmake --build . --config Release

# 4. (可选) 运行测试
ctest
```

### 构建选项

#### RBC_BUILD_EDITOR

控制是否构建编辑器（需要 Qt6）。默认值为 `ON`。

- `ON`（默认）: 构建编辑器，需要 Qt6
- `OFF`: 不构建编辑器，不查找 Qt6

示例：
```bash
# 禁用编辑器构建（不需要 Qt6）
cmake -DRBC_BUILD_EDITOR=OFF ..

# 启用编辑器构建（默认）
cmake -DRBC_BUILD_EDITOR=ON ..
```

当 `RBC_BUILD_EDITOR=OFF` 时：
- 不会查找 Qt6 库
- 不会构建 `qt_node_editor`
- 不会构建 `rbc_editor_runtime` 和 `rbc_editor`
- 不会构建 `calculator` 测试（依赖 Qt）

当 `RBC_BUILD_EDITOR=ON` 时，脚本会检查Qt的安装，可以通过`Qt6_ROOT`来传入Qt的地址，如`cmake -DQt6_ROOT=D:/tools/Qt/6.9.3 ..`

#### RBC_BUILD_PY_EXT

控制是否构建 Python 扩展模块（需要 Python3）。默认值为 `OFF`。

- `ON`: 构建 Python 扩展，需要 Python3 开发库
- `OFF`（默认）: 不构建 Python 扩展，不查找 Python3

示例：
```bash
# 启用 Python 扩展构建（需要 Python3）
cmake -DRBC_BUILD_PY_EXT=ON ..

# 禁用 Python 扩展构建（默认）
cmake -DRBC_BUILD_PY_EXT=OFF ..
```

当 `RBC_BUILD_PY_EXT=ON` 时：
- 会查找 Python3（Interpreter、Development 组件）
- 会构建 `rbc_ext_c` Python 扩展模块（.pyd 文件）

当 `RBC_BUILD_PY_EXT=OFF` 时：
- 不会查找 Python3
- 不会构建 `rbc_ext_c`

## 架构设计

### Interface/Impl 模式

CMake 构建系统实现了与 xmake `interface_target` 等价的模式。每个库目标被分为三个部分：

1. **Interface Target** (`<name>_int`): 
   - 类型：`INTERFACE` 库
   - 作用：仅传播头文件路径和依赖关系
   - 对应 xmake 的 `{name}_int__`

2. **Implementation Target** (`<name>_impl`):
   - 类型：`SHARED` 或 `STATIC` 库
   - 作用：包含实际的源文件和链接关系
   - 对应 xmake 的 `{name}_impl__`

3. **Main Target** (`<name>`):
   - 类型：`ALIAS` 或 `INTERFACE` 库
   - 作用：统一的目标名称，用于依赖引用
   - 对应 xmake 的主目标名

### 实现方式

使用 `rbc_interface_target()` 函数创建 interface/impl 分离的目标：

```cmake
rbc_interface_target(rbc_core
    INTERFACE_CALLBACK rbc_core_interface_callback
    IMPL_CALLBACK rbc_core_impl_callback
)
```

其中：
- `INTERFACE_CALLBACK`: 配置 interface 目标的函数（设置头文件路径、依赖等）
- `IMPL_CALLBACK`: 配置 implementation 目标的函数（设置源文件、链接库等）
- `NO_LINK`: 可选参数，表示该目标不进行链接（仅用于插件）

### DLL 导出/导入宏

每个库都有对应的 API 宏（如 `RBC_CORE_API`），在实现目标中定义为 `EXPORT`，在接口目标中定义为 `IMPORT`：

```cmake
# Implementation
target_compile_definitions(rbc_core_impl PRIVATE RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT)

# Interface
target_compile_definitions(rbc_core_int INTERFACE RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT)
```

## 文件结构

### CMake 辅助模块

- `cmake/rbc_interface_target.cmake`: Interface/Impl 模式实现
- `cmake/rbc_config.cmake`: 编译器配置和基本设置
- `cmake/rbc_shader_compile.cmake`: Shader 编译支持

### 第三方库配置

- `thirdparty/CMakeLists.txt`: 主配置文件
- `thirdparty/ozz_animation_cmake.cmake`: Ozz Animation 配置
- `thirdparty/tracy_cmake.cmake`: Tracy 配置
- `thirdparty/qt_node_editor_cmake.cmake`: Qt Node Editor 配置

### 核心库配置

每个库模块都有自己的 `CMakeLists.txt`:
- `rbc/core/CMakeLists.txt`: rbc_core
- `rbc/runtime/CMakeLists.txt`: rbc_runtime
- `rbc/ipc/CMakeLists.txt`: rbc_ipc
- `rbc/app/CMakeLists.txt`: rbc_app
- `rbc/node_graph/CMakeLists.txt`: rbc_node
- `rbc/render_plugin/CMakeLists.txt`: rbc_render_plugin
- `rbc/oidn_plugin/CMakeLists.txt`: oidn_plugin

### 编辑器配置

- `rbc/editor/CMakeLists.txt`: 编辑器模块入口
- `rbc/editor/runtime/CMakeLists.txt`: 编辑器运行时库
- `rbc/editor/editor/CMakeLists.txt`: 编辑器可执行文件

### 扩展和测试

- `rbc/ext_c/CMakeLists.txt`: Python 扩展模块
- `rbc/tests/CMakeLists.txt`: 测试程序主配置
- `rbc/tests/*/CMakeLists.txt`: 各个测试子模块

## 与 xmake 的对应关系

### 目标类型映射

| xmake | CMake |
|-------|-------|
| `interface_target('name', ...)` | `rbc_interface_target(name ...)` |
| `add_deps('dep')` | `target_link_libraries(target PRIVATE dep)` |
| `add_deps('dep', {links = false})` | 不链接，仅依赖 |
| `add_includedirs('dir', {public = true})` | `target_include_directories(target PUBLIC dir)` |
| `add_defines('DEF', {public = true})` | `target_compile_definitions(target PUBLIC DEF)` |
| `set_pcxxheader('header.h')` | `rbc_set_precompiled_header(target header.h)` |
| `add_rules("c++.unity_build", {batchsize = 4})` | `rbc_enable_unity_build(target 4)` |
| `add_rules('lc_basic_settings', {...})` | `rbc_apply_basic_settings(target ...)` |

### 第三方库映射

| xmake 目标 | CMake 目标 | 类型 |
|-----------|-----------|------|
| `rtm` | `rtm` | INTERFACE |
| `tiny_obj_loader` | `tiny_obj_loader` | STATIC |
| `tinyexr` | `tinyexr` | STATIC |
| `tinytiff` | `tinytiff` | STATIC |
| `tinygltf` | `tinygltf` | STATIC |
| `open_fbx` | `open_fbx` | STATIC |
| `cpp-ipc` | `cpp-ipc` | STATIC |
| `ozz_animation_runtime_static` | `ozz_animation_runtime_static` | STATIC |
| `ozz_animation_offline_static` | `ozz_animation_offline_static` | STATIC |
| `RBCTracy` | `RBCTracy` | STATIC (via interface) |
| `qt_node_editor` | `qt_node_editor` | SHARED |
| `oidn_interface` | `oidn_interface` | INTERFACE |

### LuisaCompute 集成

LuisaCompute 通过 `add_subdirectory()` 集成，所有 LuisaCompute 的目标（如 `lc-core`, `lc-runtime`, `lc-gui` 等）都可以直接使用。

## 特殊功能

### Unity Build

使用 CMake 3.16+ 的 `UNITY_BUILD` 特性实现：

```cmake
rbc_enable_unity_build(target 4)  # batchsize = 4
```

### Precompiled Headers

使用 CMake 3.16+ 的 `target_precompile_headers()`：

```cmake
rbc_set_precompiled_header(target ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)
```

### Shader 编译

Shader 编译通过自定义命令实现，调用 `build/tool/clangcxx_compiler/clangcxx_compiler.exe`：

```cmake
rbc_add_shader_compilation(target)      # 编译 dx 和 vk shaders
rbc_add_shader_hostgen(target)          # 生成 host 代码
```

### Qt 集成

使用 Qt6 的标准 CMake 模块：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
qt6_standard_project_setup()
set_target_properties(target PROPERTIES AUTOMOC ON AUTORCC ON)
```

### Python 扩展

Python 扩展使用 pybind11（来自 LuisaCompute）：

```cmake
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
# pybind11 从 LuisaCompute 获取
target_link_libraries(target PRIVATE Python3::Python)
```

## 编译器选项

### Windows (MSVC/Clang-Cl)

- `/Zc:preprocessor`: 启用符合标准的预处理器
- `/wd4244`: 禁用警告 4244
- `/GS` (Debug): 启用安全检查
- `/GS-` (Release): 禁用安全检查
- `/bigobj`: 大对象文件支持（Python 扩展）

### Linux (GCC/Clang)

- `-stdlib=libc++`: 使用 libc++（Clang）
- `-fPIC`: 位置无关代码（静态库）
- `-mfma`: FMA 指令支持（x86_64）

### macOS (Clang)

- `-no-pie`: 禁用位置无关可执行文件
- `-Wno-invalid-specialization`: 禁用特殊化警告

## 依赖关系图

```
rbc_core
  ├── lc-core
  ├── lc-vstl
  ├── lc-yyjson
  ├── RBCTracy
  └── rtm

rbc_runtime
  ├── lc-runtime
  ├── rbc_core
  ├── tinyexr
  ├── tiny_obj_loader
  ├── open_fbx
  ├── tinytiff
  ├── tinygltf
  ├── ozz_animation_runtime_static
  ├── ozz_animation_offline_static
  └── lc-volk

rbc_ipc
  ├── cpp-ipc
  └── rbc_core

rbc_app
  ├── rbc_runtime
  ├── rbc_core
  ├── rbc_render_plugin
  └── lc-gui

rbc_render_plugin
  ├── rbc_runtime
  └── (shader hostgen)

rbc_editor_runtime
  ├── rbc_core
  ├── rbc_runtime
  ├── rbc_render_plugin
  ├── rbc_app
  ├── lc-volk
  └── qt_node_editor
```

## 注意事项

1. **Python 代码生成**: 必须在 CMake 配置前完成，CMake 不会自动触发代码生成
2. **第三方库**: 假设已通过 Python 脚本下载到 `build/download/` 目录
3. **Shader 编译**: 需要 `clangcxx_compiler` 工具在 `build/tool/clangcxx_compiler/` 目录
4. **OIDN 库**: OIDN 库路径硬编码在 `rbc/oidn_plugin/CMakeLists.txt` 中，可能需要根据实际情况调整
5. **构建类型**: 使用 `--config Release` 或 `--config Debug` 指定构建类型
6. **并行构建**: CMake 支持并行构建，使用 `-j` 参数：`cmake --build . -j8`

## 故障排除

### 常见问题

1. **找不到 LuisaCompute**: 确保 `thirdparty/LuisaCompute` 目录存在
2. **Qt 找不到**: 确保 Qt6 已安装并在 PATH 中，或设置 `Qt6_DIR`
3. **Python 扩展失败**: 检查 Python 开发库是否安装
4. **Shader 编译失败**: 检查 `clangcxx_compiler` 工具是否存在

### 调试技巧

- 使用 `cmake --build . --verbose` 查看详细编译输出
- 使用 `ccmake .` 或 `cmake-gui` 交互式配置
- 检查 `CMakeCache.txt` 查看配置变量

## 未来改进

1. 自动检测 Python 代码生成需求
2. 更好的 OIDN 库路径检测
3. 支持更多平台（macOS, Linux）
4. 集成测试框架
5. 安装目标支持
