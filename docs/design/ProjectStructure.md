# RoboCute 项目目录结构

本文档描述 **RoboCute 框架源码仓库** 的目录结构。RoboCute 是一个 Python-first 的 3D AIGC / 机器人开发框架，采用自研跨平台图形引擎与节点式工作流。

> **注意**：如果你正在寻找**用户项目**（即通过 `rbc` CLI 创建的外部工作目录）的结构说明，请参阅文末 [用户项目结构](#用户项目结构) 章节。本文档主体介绍的是框架仓库本身的组织方式。

---

## 1. 概述

RoboCute 仓库采用分层设计：
- **C++ 层** (`rbc/`)：高性能运行时、渲染管线、编辑器、资源导入插件、Python 绑定
- **Python 层** (`src/`)：核心包 `robocute/`、代码生成元数据 `rbc_meta/`、C++ 绑定包装器 `rbc_ext/`
- **示例与工具** (`samples/`、`custom_nodes/`、`scripts/`)
- **文档与测试** (`docs/`、`test/`、`rbc/tests/`)

## 2. 顶层目录结构

```
RoboCute/
├── rbc/                    # C++ 核心代码库
│   ├── core/               # 底层数据结构、序列化、反射、内存管理
│   ├── runtime/            # 运行时核心（World、ECS、资源管理、动画、渲染抽象）
│   ├── node_graph/         # 节点图系统
│   ├── ipc/                # 跨进程通信
│   ├── render_plugin/      # 渲染器插件（LuisaCompute）
│   ├── importer_plugin/    # 资源导入插件（GLTF、OBJ、图像、Ozz 动画等）
│   ├── project_plugin/     # 项目管理插件（扫描、索引、缓存）
│   ├── extensions/         # Python 扩展（pybind11）
│   ├── editor/             # Qt6 桌面编辑器
│   ├── tool/               # 命令行工具（rbc_cmd）
│   ├── tests/              # C++ 单元测试与图形测试
│   └── shader/             # 着色器编译系统
├── src/                    # Python 代码库
│   ├── robocute/           # 核心 Python 包（场景、节点图、服务、动画）
│   ├── rbc_meta/           # 代码生成元数据（@reflect 定义）
│   └── rbc_ext/            # C++ 绑定生成的 Python 包装器
├── custom_nodes/           # 自定义节点扩展示例
├── samples/                # Python 示例程序
├── test/                   # Python 测试
├── docs/                   # 项目文档
├── thirdparty/             # C++ 第三方依赖（含 LuisaCompute）
├── xmake/                  # Xmake 构建脚本
├── cmake/                  # CMake 构建配置
├── scripts/                # 辅助脚本
├── build/                  # 构建输出（本地生成，不提交）
├── .meta_db/               # 运行时元数据数据库（本地生成）
├── main.py                 # 框架入口示例（演示场景与节点系统）
├── pyproject.toml          # Python 项目配置
├── xmake.lua               # Xmake 根构建脚本
├── CMakeLists.txt          # CMake 根构建脚本
├── README.md               # 项目说明
└── .python-version         # Python 版本指定
```

## 3. 详细说明

### 3.1 C++ 源码目录 (`rbc/`)

按模块组织，使用 CMake 与 Xmake 双构建系统。

#### 核心库

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `core` | `rbc_core` | shared | 底层数据结构：序列化、数学库、容器、反射、内存管理等 |
| `runtime` | `rbc_runtime` | shared | 运行时核心，包含 World 系统、ECS、资源管理、动画、图形渲染抽象、I/O、物理接口等 |
| `node_graph` | `rbc_node` | shared | 节点图系统，支持可视化节点图的定义与执行 |
| `ipc` | `rbc_ipc` | shared | 跨进程通信模块（基于 `cpp-ipc`） |

> **注意**：旧文档中提到的 `rbc_world.dll` **已不存在**。World 系统已合并到 `rbc_runtime.dll` 中，对应头文件位于 `rbc/runtime/include/rbc_world/`。

#### 插件

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `render_plugin` | `rbc_render_plugin` | shared | 渲染器插件，基于 LuisaCompute 实现光栅化/路径追踪渲染管线 |
| `importer_plugin` | `rbc_importer_plugin` | shared | 资源导入插件，支持 GLTF、OBJ、FBX、图像、Ozz 动画等格式 |
| `project_plugin` | `rbc_project_plugin` | shared | 项目管理插件，处理项目扫描、资源索引、缓存 |
| `oidn_plugin` | `oidn_plugin` | shared | （可选）Intel OpenImageDenoise 降噪插件 |

#### 编辑器

位于 `rbc/editor/`，仅在开启 `RBC_BUILD_EDITOR`（或 Xmake 的 `rbc_editor` 配置）时构建：

| 子模块 | 目标名称 | 类型 | 说明 |
|--------|----------|------|------|
| `editor/runtime` | `rbc_editor_runtime` | shared | 编辑器运行时库，提供服务层、MVVM、UI 组件、视口、节点编辑器、插件系统 |
| `editor/editor` | `rbc_editor` | binary | 编辑器主程序入口 |
| `editor/editor` | `rbc_editor_module` | shared | 编辑器核心模块（动态加载） |
| `editor/tests` | — | — | 编辑器相关测试 |

#### Python 扩展

位于 `rbc/extensions/`，仅在开启 `RBC_BUILD_PY_EXT`（或 Xmake 配置）时构建：

| 子模块 | 目标名称 | 类型 | 说明 |
|--------|----------|------|------|
| `ext_c` | `rbc_ext_c` | `.pyd` | C++ 核心功能的 Pybind11 绑定 |
| `lcapi_c` | `lcapi_c` | `.pyd` | LuisaCompute 的 Python 绑定扩展 |
| `common` | `rbc_ext_common` | static | 扩展模块公共代码 |

#### 工具与测试

| 模块 | 目标名称 | 类型 | 说明 |
|------|----------|------|------|
| `tool/rbc_cmd` | `rbc` | binary | 命令行工具（CLI），支持创建用户项目、下载模板 |
| `tests/` | `test_core`、`test_anim`、`test_graphics`、`test_model`、`test_project`、`test_skeleton`、`test_anim_sequence` 等 | binary | C++ 单元测试与图形测试 |
| `shader/` | — | — | 着色器编译系统（`hostgen`、DXIL/SPIR-V 编译、`.cache`） |

### 3.2 Python 源码目录 (`src/`)

| 子目录 | 说明 |
|--------|------|
| `robocute/` | 核心 Python 包，暴露给用户的公共 API。包含场景管理 (`scene.py`)、节点图 (`graph.py`)、服务 (`service.py`)、动画 (`animation.py`)、编辑器服务 (`editor_service.py`) 等 |
| `rbc_meta/` | 代码生成元数据。使用 `@reflect` 装饰器定义 C++ ↔ Python 的接口契约，生成 pybind11 绑定代码。关键文件如 `types/world_interface.py` |
| `rbc_ext/` | 代码生成输出目录。包含 `generated/world.py` 等自动生成的 Python 包装器，将 C++ 类型暴露为 Python 类 |

### 3.3 示例与扩展

| 目录 | 说明 |
|------|------|
| `samples/` | Python 示例程序，如 `app_graphics_scene.py`（图形场景）、`app_anim_scene.py`（动画场景）、`app_uipc_physics.py`（物理仿真）等 |
| `custom_nodes/` | 自定义节点扩展示例，如 `animation_nodes.py`、`text2image_nodes.py`、`uipc.py` |
| `scripts/` | 辅助脚本（如代码生成、资源安装等） |

### 3.4 构建系统文件

| 文件 | 说明 |
|------|------|
| `xmake.lua` | Xmake 根构建脚本，定义所有 C++ 目标、依赖、编译选项 |
| `CMakeLists.txt` | CMake 根构建脚本，与 Xmake 双轨并行 |
| `xmake/` | Xmake 辅助脚本与模块配置 |
| `cmake/` | CMake 辅助模块与查找脚本 |
| `build/` | 构建输出目录（本地生成，**不应提交到版本控制**） |

### 3.5 文档与测试

| 目录 | 说明 |
|------|------|
| `docs/` | 项目文档，包含设计文档 (`design/`)、开发指南 (`dev/`)、教程 (`tutorials/`)、用户指南 (`user-guide/`)、开发日志 (`devlog/`) 等 |
| `test/` | Python 测试（使用 pytest） |
| `rbc/tests/` | C++ 测试（使用 doctest） |

### 3.6 根目录关键文件

| 文件 | 说明 |
|------|------|
| `main.py` | 框架入口示例文件，演示如何初始化场景、加载网格、创建实体、注册服务并启动服务器 |
| `pyproject.toml` | Python 项目配置，定义依赖、开发工具、脚本命令（如 `uv run gen`、`uv run pre-pack`） |
| `.python-version` | 指定 Python 版本（>=3.13） |
| `uv.lock` | uv 包管理器锁文件 |

---

## 4. 用户项目结构

当你使用 `rbc` CLI 创建新项目时，RoboCute 会从 GitHub 下载外部模板仓库 `RoboCute/rbc-project-default`，生成如下结构的用户工作目录：

```
rbc-project-default/
├── assets/                # 项目资源文件（模型、材质、纹理、场景）
├── datasets/              # 数据集目录
├── docs/                  # 项目文档
├── library/               # 资源库（自动生成的缓存/索引）
├── pretrained/            # 预训练模型目录
├── .temp_db/              # 临时数据库目录
├── main.py                # 项目入口文件
├── pyproject.toml         # Python 项目配置
├── rbc_project.json       # RoboCute 项目配置
├── .gitignore             # Git 忽略配置
├── .rbcignore             # RoboCute 忽略配置
└── README.md              # 项目说明文档
```

> 用户项目**不在本仓库内**。运行示例时请通过 `-p <path_to_your_project>` 指定项目路径，例如：
> ```bash
> uv run samples/app_graphics_scene.py -p /path/to/your/project
> ```

---

## 5. Project API 使用指南

`Project` 类用于管理项目资源和导入外部资源。该类通过 C++ 绑定提供，位于 `robocute.rbc_ext.generated.world` 模块（也可通过 `robocute.world` 访问）。

### 5.1 创建和初始化

```python
from robocute.rbc_ext.generated.world import Project

# 创建 Project 实例
project = Project()

# 初始化项目，指定资源根目录
project.init("./assets")

# 扫描项目资源
project.scan_project()
```

### 5.2 资源导入

```python
# 从文件导入 3D 模型
mesh_resource = project.import_mesh("./assets/models/bunny.obj")

# 导入纹理，指定 mip 层级和是否转换为虚拟纹理
# mip_level: Mipmap 层级 (-1 表示自动生成所有层级)
# to_vt: 是否转换为虚拟纹理 (Virtual Texture)
texture_resource = project.import_texture(
    "./assets/textures/albedo.png", mip_level=-1, to_vt=True
)

# 从材质定义文件导入材质
material_resource = project.import_material("./assets/materials/basic_mat.mat")

# 从场景文件导入场景
# extra_meta: 额外的元数据（JSON 字符串格式）
scene = project.import_scene("./assets/scenes/test_scene.scene", extra_meta="{}")

# 导入动画相关资源（v0.2+ 新增）
skeleton = project.import_skeleton("./assets/animations/character.ozz")
skin = project.import_skin("./assets/animations/character.ozz")
anim = project.import_anim_sequence("./assets/animations/run.ozz")
```

### 5.3 资源获取

```python
from robocute.rbc_ext.generated.world import GUID

# 使用 GUID 获取资源
# load_content_async: 是否异步加载资源内容
resource = project.get_resource(GUID("..."), load_content_async=True)

# 获取指定类型和路径的文件元数据
file_meta = project.get_file_meta(GUID("..."), "./assets/models/bunny.obj")
```

### 5.4 API 参考

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `init(assets_root_dir)` | `assets_root_dir: str` | `None` | 初始化项目，设置资源根目录 |
| `scan_project()` | - | `None` | 扫描项目，更新资源索引 |
| `import_mesh(path)` | `path: str` | `MeshResource` | 从文件导入 3D 网格模型 |
| `import_texture(path, mip_level, to_vt)` | `path: str`, `mip_level: int`, `to_vt: bool` | `TextureResource` | 导入纹理图像 |
| `import_material(path)` | `path: str` | `MaterialResource` | 从 .mat 文件导入材质 |
| `import_scene(path, extra_meta)` | `path: str`, `extra_meta: str` | `Scene` | 从 .scene 文件导入场景 |
| `import_skeleton(path)` | `path: str` | `SkeletonResource` | 导入骨骼资源 |
| `import_skin(path)` | `path: str` | `SkinResource` | 导入蒙皮资源 |
| `import_anim_sequence(path)` | `path: str` | `AnimSequenceResource` | 导入动画序列 |
| `get_resource(guid, load_content_async)` | `guid: GUID`, `load_content_async: bool` | `Resource` | 通过 GUID 获取已导入的资源 |
| `get_file_meta(type_id, dest_path)` | `type_id: GUID`, `dest_path: str` | `FileMeta` | 获取文件的元数据信息 |

### 5.5 使用示例（完整流程）

```python
from robocute.rbc_ext.generated.world import Project, RBCContext

# 初始化项目
project = Project()
project.init("./assets")
project.scan_project()

# 导入资源
bunny_mesh = project.import_mesh("./assets/bunny.obj")
albedo_tex = project.import_texture("./assets/albedo.png", mip_level=-1, to_vt=True)
material = project.import_material("./assets/basic_mat.mat")
scene = project.import_scene("./assets/test_scene.scene", "{}")

# 在渲染上下文中使用资源
ctx = RBCContext()
# 注意：init_device 需要 4 个参数，最后一个 compactible 为 bool
ctx.init_device("cuda", "./", "./shaders", compactible=True)
ctx.init_world("./library/meta", "./library/binary")
```

### 5.6 注意事项

1. **初始化顺序**: 必须先调用 `init()` 设置资源根目录，然后才能导入资源
2. **扫描项目**: 导入新资源后，建议调用 `scan_project()` 更新资源索引
3. **路径格式**: 使用相对路径或绝对路径均可，但建议统一使用相对路径
4. **资源缓存**: 导入的资源会自动缓存到 `library/` 目录，重复导入时会使用缓存
5. **上下文初始化**: `RBCContext.init_device()` 的 `compactible` 参数不可省略

---

## 6. 开发工作流

### 6.1 环境准备

```bash
# 1. 同步 Python 环境
uv sync --extra dev

# 2. 准备项目依赖
uv run prepare -y

# 3. 生成代码（修改 rbc_meta 后必须执行）
uv run gen
```

### 6.2 构建

```bash
# 配置（首次）
xmake f -m debug -c

# 构建全部
xmake

# 构建特定目标
xmake rbc_runtime
```

### 6.3 运行

```bash
# 启动 Python 开发服务器
uv run main.py

# 运行 C++ 编辑器（需另开终端）
xmake run rbc-editor
```

### 6.4 测试

```bash
# Python 测试
uv run pytest

# C++ 测试
xmake run test_core
xmake run test_graphics
```

---

## 7. 最佳实践

1. **版本控制**：只提交源代码和原始资源，不提交生成的缓存文件（`build/`、`.meta_db/`、`__pycache__/`、`.xmake/` 等）
2. **代码生成**：修改 `src/rbc_meta/` 或反射宏后，务必运行 `uv run gen`
3. **构建顺序**：先 `uv run gen` 再 `xmake`，否则可能因缺失生成代码而编译失败
4. **资源命名**：使用有意义的文件名，避免空格和特殊字符
5. **依赖管理**：使用 `uv` 工具管理 Python 依赖，使用 `xmake` 管理 C++ 依赖
