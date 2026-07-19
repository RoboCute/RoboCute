# RoboCute Architecture

本文档描述 RoboCute 的整体架构与模块组成。项目采用 **Python-First** 设计，核心逻辑在 Python 中运行，C++ 层提供高性能运行时、图形渲染与可选的桌面编辑器。

---

## 1. 顶层目录结构

```
RoboCute/
├── rbc/              # C++ 核心代码库
├── src/              # Python 代码库
├── samples/          # Python 示例程序
├── thirdparty/       # 第三方依赖（含 LuisaCompute）
├── docs/             # 文档
├── cmake/            # CMake 构建配置
├── xmake/            # Xmake 构建配置
└── build/            # 构建输出目录
```

---

## 2. C++ 层 (`rbc/`)

C++ 代码按模块组织，使用 **CMake** 与 **Xmake** 双构建系统。

### 2.1 核心库 (Core Libraries)

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `core` | `rbc_core` | shared | 底层数据结构：序列化、数学库、容器、反射、内存管理等 |
| `runtime` | `rbc_runtime` | shared | **运行时核心**，包含世界系统 (`rbc_world`)、ECS、资源管理、动画、图形渲染抽象、I/O、物理接口等 |
| `node_graph` | `rbc_node` | shared | 节点图系统，支持可视化节点图的定义与执行 |
| `ipc` | `rbc_ipc` | shared | 跨进程通信模块（基于 `cpp-ipc`） |

> **注意**：旧文档中提到的 `rbc_world.dll` **已不存在**。World 系统（场景、资源、Entity-Component）已合并到 `rbc_runtime.dll` 中，对应头文件位于 `rbc/runtime/include/rbc_world/`。

### 2.2 插件 (Plugins)

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `render_plugin` | `rbc_render_plugin` | shared | 渲染器插件，基于 LuisaCompute 实现光栅化/路径追踪渲染管线 |
| `importer_plugin` | `rbc_importer_plugin` | shared | 资源导入插件，支持 GLTF、OBJ、FBX、图像、Ozz 动画等格式 |
| `project_plugin` | `rbc_project_plugin` | shared | 项目管理插件，处理项目扫描、资源索引、缓存 |
| `oidn_plugin` | `oidn_plugin` | shared | （可选）Intel OpenImageDenoise 降噪插件 |

### 2.3 编辑器 (Editor)

位于 `rbc/editor/`，仅在开启 `RBC_BUILD_EDITOR`（或 Xmake 的 `rbc_editor` 配置）时构建：

| 子模块 | 目标名称 | 类型 | 说明 |
|--------|----------|------|------|
| `editor/runtime` | `rbc_editor_runtime` | shared | 编辑器运行时库，提供服务层、MVVM、UI 组件、视口、节点编辑器、插件系统 |
| `editor/editor` | `rbc_editor` | binary | 编辑器主程序入口 |
| `editor/editor` | `rbc_editor_module` | shared | 编辑器核心模块（动态加载） |
| `editor/editor` | `rbc_editor_test_module` | shared | 编辑器测试模块 |
| `editor/tests` | — | — | 编辑器相关测试 |

### 2.4 Python 扩展 (Extensions)

位于 `rbc/extensions/`，仅在开启 `RBC_BUILD_PY_EXT`（或 Xmake 配置）时构建：

| 子模块 | 目标名称 | 类型 | 说明 |
|--------|----------|------|------|
| `ext_c` | `rbc_ext_c` | `.pyd` | C++ 核心功能的 Pybind11 绑定 |
| `lcapi_c` | `lcapi_c` | `.pyd` | LuisaCompute 的 Python 绑定扩展 |
| `common` | `rbc_ext_common` | static | 扩展模块公共代码 |

### 2.5 工具与测试

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `tool/rbc_cmd` | `rbc` | binary | 命令行工具（CLI） |
| `tests/` | `test_core`, `test_anim`, `test_graphics`, `test_model`, `test_project`, `test_skeleton`, `test_anim_sequence`, ... | binary | C++ 单元测试与图形测试 |
| `shader/` | — | — | 着色器编译系统（`hostgen`、DXIL/SPIR-V 编译、`.cache`） |

---

## 3. Python 层 (`src/`)

Python 代码是 RoboCute 的 **单一事实来源（Single Source of Truth）**，所有场景数据、节点逻辑、算法编排都在此层实现。

| 模块 | 说明 |
|------|------|
| `robocute/` | Python 核心库，暴露主要 API：<br>• `scene.py` — 场景、Entity、Component 的高层封装<br>• `graph.py`, `executor.py`, `node_base.py`, `node_registry.py` — 节点图定义与执行<br>• `animation.py` — 动画关键帧与序列<br>• `resource.py` — 资源管理工具<br>• `editor_service.py`, `service.py`, `node_graph_service.py` — 服务端与编辑器通信<br>• `app.py` — `App` 单例，封装渲染窗口、设备初始化、主循环 |
| `rbc_meta/` | Python/C++ 通用 codegen 入口，元信息结构定义与函数分析器 |
| `rbc_ext/` | C++ 生成绑定的 Python 封装层：<br>• `generated/world.py` — `world` 模块的 Python 侧接口<br>• `luisa/` — LuisaCompute Python API 的封装 |
| `uipc/` | UIPC（Unified Incremental Potential Contact）物理引擎的 Python 集成 |
| `scripts/` | 构建与开发辅助脚本 |
| `rbc_execution/` | 执行系统预留目录（当前内容极少） |

### 3.1 Python 入口 (`robocute`)

`src/robocute/__init__.py` 导出了框架的主要公共 API，包括：

- `Scene`, `Entity`, `TransformComponent`, `RenderComponent`
- `NodeGraph`, `GraphDefinition`, `NodeDefinition`, `NodeConnection`
- `GraphExecutor`, `ExecutionStatus`
- `Server`, `Service`, `EditorService`, `NodeGraphService`
- `world`（来自 `rbc_ext` 的底层 C++ 绑定封装）
- `app`（`App` 单例，用于快速创建可视化窗口）

---

## 4. 架构关系图

TBD

---

## 5. 关键设计原则

1. **Python-First**：所有场景数据与业务逻辑在 Python 层定义，C++ 仅作为高性能运行时与可视化后端。
2. **可选编辑器**：`rbc_editor` 是一个独立的 Qt6 二进制程序，通过服务层与 Python 进程通信；可以完全不启动编辑器进行 Headless 渲染/仿真。
3. **插件化**：渲染、导入、项目管理、降噪均以插件形式存在，便于扩展与按需加载。
4. **双构建系统**：同时支持 **Xmake**（主构建系统）与 **CMake**（兼容部分 IDE 与 CI 场景）。

---

## 6. 文档索引

- [设计总览 / Overview](Overview.md)
- [项目目录结构 / ProjectStructure](ProjectStructure.md)
- [构建指南 / BUILD](../BUILD.md)
- [开发日志 / devlog](../devlog/)
