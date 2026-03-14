# RoboCute 项目目录结构

本文档描述了标准 RoboCute 项目的目录结构和各目录/文件的用途。

## 概述

RoboCute 是一个用于 AIGC、机器人学和仿真的可视化编程框架。项目采用特定的目录结构来组织资源、数据集、预训练模型和项目配置。

## 目录结构

```
rbc-project-default/
├── .temp_db/              # 临时数据库目录
├── assets/                # 项目资源文件
├── datasets/              # 数据集目录
├── docs/                  # 项目文档
├── library/               # 资源库（自动生成的缓存/索引）
├── pretrained/            # 预训练模型目录
├── .gitignore             # Git 忽略配置
├── .python-version        # Python 版本指定
├── .rbcignore             # RoboCute 忽略配置
├── main.py                # 项目入口文件
├── pyproject.toml         # Python 项目配置
├── rbc_project.json       # RoboCute 项目配置
└── README.md              # 项目说明文档
```

## 详细说明

### 根目录文件

#### `rbc_project.json`
RoboCute 项目的核心配置文件，定义了项目元数据、路径配置和运行时设置：

| 字段 | 说明 |
|------|------|
| `name` | 项目名称 |
| `version` | 项目版本 |
| `rbc_version` | RoboCute 框架版本要求 |
| `author` | 项目作者 |
| `description` | 项目描述 |
| `paths` | 各资源路径配置 |
| `config` | 运行时配置（默认场景、启动图、图形后端等） |
| `metadata` | 项目元数据（标签、许可证、仓库地址） |

#### `main.py`
项目入口文件，包含示例代码展示如何使用 RoboCute 的节点图系统：
- 创建节点图（GraphDefinition）
- 定义节点（NodeDefinition）和连接（NodeConnection）
- 验证和执行节点图
- 进度回调处理

#### `pyproject.toml`
标准 Python 项目配置文件：
- 项目名称和版本
- Python 版本要求（>=3.13）
- 项目依赖


#### `.rbcignore`
RoboCute 特定的忽略配置文件（当前为空）。

### 资源目录 (`assets/`)

存放项目使用的各种资源文件：

| 文件类型 | 扩展名 | 说明 |
|----------|--------|------|
| 3D 模型 | `.obj` | Wavefront OBJ 格式模型文件 |
| 材质 | `.mat` | RoboCute 材质定义文件 |
| 场景 | `.scene` | RoboCute 场景文件（JSON 格式） |
| 纹理 | `.png` | 图像纹理文件 |
| 元数据 | `.rbcmt` | RoboCute 元数据文件（自动生成的资源描述） |

**示例资源：**
- `bunny.obj` - 斯坦福兔子模型
- `cornell_box.obj` - Cornell Box 测试场景
- `test_scene.scene` - 测试场景配置
- `basic_mat.mat`, `left_wall_mat.mat` 等 - 材质定义

### 数据集目录 (`datasets/`)

用于存放训练数据、测试数据等数据集。默认为空目录，包含 `.gitkeep` 文件。

### 文档目录 (`docs/`)

项目相关文档存放位置：
- `init_project.md` - 项目初始化指南

### 资源库目录 (`library/`)

**自动生成**的资源索引和缓存目录，包含：

| 文件/目录 | 说明 |
|-----------|------|
| `.meta_db/` | 元数据数据库（LMDB 格式） |
| `*.rbcb` | RoboCute 二进制缓存文件 |
| `scene.rbc` | 场景资源索引 |

**注意：** 此目录由 RoboCute 自动生成和维护，通常不应手动修改，也不应提交到版本控制。

### 预训练模型目录 (`pretrained/`)

存放预训练模型文件。默认为空目录，包含 `.gitkeep` 文件。

### 临时数据库目录 (`.temp_db/`)

运行时临时数据存储目录。

## 工作流程

### 项目初始化

1. 克隆项目模板
2. 运行 `uv sync` 安装依赖
3. RoboCute 会自动生成 `library/` 目录和相关缓存

### 资源管理

1. 将资源文件放入 `assets/` 目录
2. RoboCute 会自动生成对应的 `.rbcmt` 元数据文件
3. 资源索引存储在 `library/` 目录中

### 开发工作流

1. 在 `main.py` 中编写节点图逻辑
2. 在 `assets/` 中添加场景和资源
3. 使用 `rbc_project.json` 配置项目设置

## 文件关联关系

```
rbc_project.json
    ├── paths.assets → assets/
    ├── paths.datasets → datasets/
    ├── paths.pretrained → pretrained/
    ├── paths.intermediate → .rbc/ (运行时生成)
    └── config.default_scene → assets/test_scene.scene

assets/*
    └── *.rbcmt (自动生成的元数据)
        └── library/.meta_db/ (元数据索引)
```

## Project API 使用指南

`Project` 类是 RoboCute 中用于管理项目资源和导入外部资源的核心 API。该类通过 C++ 绑定提供，位于 `robocute.rbc_ext.generated.world_v2` 模块。

### 创建和初始化

```python
from robocute.rbc_ext.generated.world_v2 import Project

# 创建 Project 实例
project = Project()

# 初始化项目，指定资源根目录
project.init("./assets")

# 扫描项目资源
project.scan_project()
```

### 资源导入

Project 类提供了多种导入外部资源的方法：

#### 导入网格 (Mesh)

```python
# 从文件导入 3D 模型
mesh_resource = project.import_mesh("./assets/models/bunny.obj")
```

#### 导入纹理 (Texture)

```python
# 导入纹理，指定 mip 层级和是否转换为虚拟纹理
# mip_level: Mipmap 层级 (-1 表示自动生成所有层级)
# to_vt: 是否转换为虚拟纹理 (Virtual Texture)
texture_resource = project.import_texture("./assets/textures/albedo.png", mip_level=-1, to_vt=True)
```

#### 导入材质 (Material)

```python
# 从材质定义文件导入材质
material_resource = project.import_material("./assets/materials/basic_mat.mat")
```

#### 导入场景 (Scene)

```python
# 从场景文件导入场景
# extra_meta: 额外的元数据（JSON 字符串格式）
scene = project.import_scene("./assets/scenes/test_scene.scene", extra_meta="{}")
```

### 资源获取

#### 通过 GUID 获取资源

```python
from robocute.rbc_ext.generated.world_v2 import GUID

# 使用 GUID 获取资源
# load_content_async: 是否异步加载资源内容
resource = project.get_resource(GUID("..."), load_content_async=True)
```

#### 获取文件元数据

```python
# 获取指定类型和路径的文件元数据
file_meta = project.get_file_meta(GUID("..."), "./assets/models/bunny.obj")
```

### API 参考

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `init(assets_root_dir)` | `assets_root_dir: str` | `None` | 初始化项目，设置资源根目录 |
| `scan_project()` | - | `None` | 扫描项目，更新资源索引 |
| `import_mesh(path)` | `path: str` | `MeshResource` | 从文件导入 3D 网格模型 |
| `import_texture(path, mip_level, to_vt)` | `path: str`, `mip_level: int`, `to_vt: bool` | `TextureResource` | 导入纹理图像 |
| `import_material(path)` | `path: str` | `MaterialResource` | 从 .mat 文件导入材质 |
| `import_scene(path, extra_meta)` | `path: str`, `extra_meta: str` | `Scene` | 从 .scene 文件导入场景 |
| `get_resource(guid, load_content_async)` | `guid: GUID`, `load_content_async: bool` | `Resource` | 通过 GUID 获取已导入的资源 |
| `get_file_meta(type_id, dest_path)` | `type_id: GUID`, `dest_path: str` | `FileMeta` | 获取文件的元数据信息 |

### 使用示例

完整的工作流程示例：

```python
from robocute.rbc_ext.generated.world_v2 import Project, RBCContext

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
ctx.init_device("cuda", "./", "./shaders")
ctx.init_world("./library/meta", "./library/binary")
```

### 注意事项

1. **初始化顺序**: 必须先调用 `init()` 设置资源根目录，然后才能导入资源
2. **扫描项目**: 导入新资源后，建议调用 `scan_project()` 更新资源索引
3. **路径格式**: 使用相对路径或绝对路径均可，但建议统一使用相对路径
4. **资源缓存**: 导入的资源会自动缓存到 `library/` 目录，重复导入时会使用缓存
5. **内存管理**: Project 实例会在销毁时自动释放底层资源，也可手动调用 `dispose()`

## 最佳实践

1. **版本控制**：只提交源代码和原始资源，不提交生成的缓存文件
2. **资源命名**：使用有意义的文件名，避免空格和特殊字符
3. **项目配置**：根据实际路径修改 `rbc_project.json` 中的路径配置
4. **依赖管理**：使用 `uv` 工具管理 Python 依赖
5. **资源组织**：按类型或功能在 `assets/` 中创建子目录组织资源
