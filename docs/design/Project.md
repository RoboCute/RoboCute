# RoboCute Project System - Design Overview

## 概述 / Overview

RoboCute 项目系统负责管理项目的目录结构、资源导入、配置管理等。本文档是项目系统的设计概览，详细的规范和实现请参考相关文档。

The RoboCute project system manages project directory structure, resource import, and configuration management. This document provides a design overview, with detailed specifications and implementations in related documents.

---

## 相关文档 / Related Documents

### 📋 完整规范 / Complete Specification
- **[ProjectStructure.md](ProjectStructure.md)** - 完整的项目文件结构规范
  - 项目目录结构
  - 配置文件格式（rbc_project.json）
  - 资源文件格式（.rbcb, .rbcbundle）
  - 数据库表结构（resource_registry.db）
  - 场景和节点图文件格式
  - 版本控制建议

### 🔧 实现指南 / Implementation Guides

- **[resource_management.md](../dev/resource_management.md)** - 资源管理系统实现
  - 资源生命周期
  - 导入器接口和实现
  - 资源注册数据库
  - 资源导入管理器
  - Python API 和命令行工具

- **[project_initialization.md](../dev/project_initialization.md)** - 项目初始化和管理
  - 项目创建流程
  - 项目管理 API
  - 命令行工具
  - 项目模板系统
  - 配置管理
  - 项目迁移和升级

---

## 快速参考 / Quick Reference

### 项目目录结构 / Project Directory Structure

```
MyRoboCuteProject/
├── rbc_project.json          # 项目配置文件
├── .rbcignore                 # 忽略文件配置
├── assets/                    # 原始资产（GLTF, 纹理等）
  ├── scenes/                    # 场景文件
  ├── graphs/                    # 节点图文件
  ├── scripts/                   # 自定义脚本和节点
├── docs/                      # 项目文档
├── datasets/                  # 训练数据集
├── pretrained/                # 预训练模型权重
└── .rbc/                      # 中间文件目录（不提交到版本控制）
    ├── resources/             # 运行时资源
    │   ├── resource_registry.db
        | xxx.rbcb
        | xxx.rbch
        | ....
    ├── cache/                 # 缓存文件
    │   ├── shaders/
    │   ├── thumbnails/
    │   └── temp/
    ├── logs/                  # 日志
    │   └── log.db
    └── out/                   # 输出目录
        ├── renders/
        ├── exports/
        └── datasets/
```

### 配置层级 / Configuration Hierarchy

#### 1. 应用级配置 / Application-Level Config
存储位置：
- Windows: `%APPDATA%/RoboCute/config.json`
- Linux/macOS: `~/.config/robocute/config.json`

用途：
- 运行时路径（executable_path）
- 着色器搜索路径（shader_search_paths）
- 编辑器偏好设置（theme, font_size, etc.）
- 最近打开的项目列表
- 默认项目路径

#### 2. 项目级配置 / Project-Level Config
存储位置：`{project_root}/rbc_project.json`

用途：
- 项目元数据（name, version, author, description）
- 目录路径配置
- 默认场景和启动图
- 渲染后端选择
- 资源版本号

### 核心概念 / Core Concepts

#### 1. 资源 GUID / Resource GUID
- 每个资源有唯一的 GUID（128-bit UUID）
- GUID 在资源的 `.meta` 文件中定义
- 使用 GUID 引用资源，而不是路径

#### 2. 资源导入流程 / Resource Import Pipeline
```
Asset File → Importer → Resource → Serialize → .rbcb → Registry DB
```

#### 3. 资源状态 / Resource Status
- `Unloaded`: 未加载
- `Loading`: 加载中
- `Loaded`: 已加载

#### 4. 资源注册数据库 / Resource Registry Database
- SQLite 数据库
- 记录资源元信息
- 管理依赖关系
- 追踪导入缓存

---

## 实现路径 / Implementation Roadmap

### Phase 1: 基础设施 / Infrastructure ✅
- [x] Resource 基类
- [x] BaseObject 系统
- [x] Entity-Component 系统
- [x] 序列化框架

### Phase 2: 资源导入系统 / Resource Import System 🚧
- [ ] 资源导入器接口（`IResourceImporter`）
- [ ] 导入器注册表（`ResourceImporterRegistry`）
- [ ] 资源注册数据库（`ResourceRegistryDB`）
- [ ] 资源导入管理器（`ResourceImportManager`）
- [ ] 内置导入器
  - [x] GLTF Mesh Importer
  - [x] GLTF Skeleton Importer
  - [x] GLTF Skin Importer
  - [x] GLTF Animation Importer
  - [ ] Texture Importer
  - [ ] Material Importer

### Phase 3: 项目管理系统 / Project Management System 🔜
- [ ] Project 类
- [ ] 项目创建工具
- [ ] 项目加载和保存
- [ ] 项目验证
- [ ] 项目模板系统

### Phase 4: Python API 和 CLI 工具 / Python API & CLI Tools 🔜
- [ ] Python bindings for Project
- [ ] Python bindings for ResourceImportManager
- [ ] CLI 工具
  - [ ] `rbc project create`
  - [ ] `rbc project info`
  - [ ] `rbc project validate`
  - [ ] `rbc import asset`
  - [ ] `rbc import dir`
  - [ ] `rbc pack`
  - [ ] `rbc unpack`

### Phase 5: 编辑器集成 / Editor Integration 🔜
- [ ] 项目浏览器
- [ ] 资源浏览器
- [ ] 资源导入 UI
- [ ] 项目设置面板

---

## 使用示例 / Usage Examples

### 创建新项目 / Create New Project

```bash
# 使用 CLI
rbc project create MyRobotProject \
    --name "My Robot Project" \
    --author "John Doe" \
    --description "A robot simulation project" \
    --git
```

```python
# 使用 Python API
import robocute as rbc

project = rbc.Project.create(
    path="D:/Projects/MyRobotProject",
    name="My Robot Project",
    author="John Doe",
    description="A robot simulation project",
    version="0.1.0"
)
```

### 导入资源 / Import Assets

```bash
# 导入单个资产
rbc import asset --project MyRobotProject --asset assets/models/robot.gltf

# 导入目录
rbc import dir --project MyRobotProject --directory assets/ --recursive
```

```python
# 使用 Python API
import robocute as rbc

project = rbc.Project.load("D:/Projects/MyRobotProject")
importer = project.import_manager()

# 导入单个资产
result = importer.import_asset("assets/models/robot.gltf")
print(f"Imported {len(result['resources'])} resources")

# 导入目录
result = importer.import_directory("assets", recursive=True)
```

### 加载资源 / Load Resources

```python
import robocute as rbc

# 通过 GUID 加载资源
resource = rbc.load_resource("a1b2c3d4-e5f6-7890-abcd-ef1234567890")

# 等待异步加载完成
await resource.await_loading()

# 使用资源
if isinstance(resource, rbc.MeshResource):
    resource.install()
    mesh = resource.device_mesh()
```

---

## 下一步 / Next Steps

1. **实现资源导入管理器**: 参考 [resource_management.md](../dev/resource_management.md)
2. **实现项目管理系统**: 参考 [project_initialization.md](../dev/project_initialization.md)
3. **编写单元测试**: 测试资源导入、项目创建等核心功能
4. **Python Bindings**: 为 C++ API 提供 Python 绑定
5. **CLI 工具**: 实现命令行工具，方便用户使用
6. **文档和示例**: 编写用户文档和示例项目

---

## 参考 / References

- [Architecture.md](Architecture.md) - RoboCute 整体架构
- [RBCResource.md](RBCResource.md) - 资源管理设计
- [Serde.md](Serde.md) - 序列化系统设计

---

**状态**: 设计完成，实现中  
**最后更新**: 2025-12-31  
**作者**: RoboCute Team