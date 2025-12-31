# RoboCute Project Structure Specification

## 概述 / Overview

本文档定义了 RoboCute 项目的完整文件结构规范，包括原始资产管理、运行时资源、版本控制、配置管理等。

This document defines the complete file structure specification for RoboCute projects, including raw asset management, runtime resources, version control, and configuration management.

## 项目根目录结构 / Root Directory Structure

```
MyRoboCuteProject/
├── rbc_project.json          # 项目入口文件 / Project entry file
├── .rbcignore                 # 忽略文件配置 / Ignore file configuration
├── README.md                  # 项目说明文档 / Project readme
├── assets/                    # 所有原始资产（包括场景、脚本、模型等）
│   ├── models/               # 3D 模型资产
│   ├── animations/           # 动画资产
│   ├── textures/             # 纹理资产
│   ├── scenes/               # 场景定义文件
│   ├── graphs/               # 节点图定义文件
│   ├── scripts/              # 自定义脚本和节点
│   ├── audio/                # 音频资产
│   └── ...                   # 其他资产类型
├── docs/                      # 项目文档 / Project documentation
├── datasets/                  # 训练数据集 / Training datasets
├── pretrained/                # 预训练模型权重 / Pretrained model weights
└── .rbc/                      # 项目中间文件目录（不应提交到版本控制）
    ├── resources/             # 导入后的运行时资源（扁平结构，使用 .rbcm 标识类型）
    ├── cache/                 # 各种缓存文件 / Cache files
    ├── logs/                  # 日志和数据库 / Logs and databases
    └── out/                   # 默认输出目录 / Default output directory
```

---

## 1. 项目配置文件 / Project Configuration Files

### 1.1 `rbc_project.json` - 项目入口文件

项目的根配置文件，定义项目元信息和路径配置。

**文件格式 / File Format:**

```json
{
  "name": "MyRoboCuteProject",
  "version": "0.1.0",
  "rbc_version": "0.2.0",
  "author": "Your Name",
  "description": "Project description",
  "created_at": "2025-12-31T00:00:00Z",
  "modified_at": "2025-12-31T12:00:00Z",
  
  "paths": {
    "assets": "assets",
    "docs": "docs",
    "datasets": "datasets",
    "pretrained": "pretrained",
    "intermediate": ".rbc"
  },
  
  "config": {
    "default_scene": "assets/scenes/main.rbcscene",
    "startup_graph": "assets/graphs/startup.rbcgraph",
    "backend": "dx",
    "resource_version": "1"
  },
  
  "metadata": {
    "tags": ["robotics", "simulation"],
    "license": "Apache-2.0",
    "repository": "https://github.com/user/project"
  }
}
```

**字段说明 / Field Descriptions:**

- `name`: 项目名称
- `version`: 项目版本号（语义化版本）
- `rbc_version`: 所需的 RoboCute 最低版本
- `author`: 项目作者
- `description`: 项目描述
- `paths`: 项目内各目录的相对路径配置
- `config.default_scene`: 默认启动场景
- `config.startup_graph`: 启动时执行的节点图
- `config.backend`: 渲染后端选择（dx/vk/cuda）
- `config.resource_version`: 资源版本号，用于缓存失效

---

### 1.2 `.rbcignore` - 忽略文件配置

类似 `.gitignore`，定义哪些文件不应该被项目管理或打包。

**示例 / Example:**

```gitignore
# RoboCute intermediate files
.rbc/
*.rbcb.tmp
*.log

# OS files
.DS_Store
Thumbs.db

# IDE files
.vscode/
.idea/
*.swp

# Large temporary files
*.tmp
*.cache
```

---

## 2. 原始资产目录 / Raw Assets Directory

### 2.1 `assets/` - 所有原始资产

存放项目的**所有**原始资产，包括从 DCC 工具导出的文件、场景定义、节点图、自定义脚本等。

**设计理念**:
- **统一管理**: 所有资产都在 `assets/` 下，便于导入、追踪和版本控制
- **自动 Meta**: 复制到 `assets/` 的文件会自动生成 `.meta` 文件，防止重复导入
- **类型无关**: 不限制资产类型，可以添加任何项目需要的资产

**目录结构 / Directory Structure:**

```
assets/
├── models/                    # 3D 模型 / 3D models
│   ├── characters/
│   │   ├── robot_a.gltf
│   │   ├── robot_a.gltf.meta        # 自动生成的 meta 文件
│   │   ├── robot_a_textures/
│   │   │   ├── base_color.png
│   │   │   ├── base_color.png.meta
│   │   │   ├── normal.png
│   │   │   ├── normal.png.meta
│   │   │   └── roughness.png
│   │   └── robot_a.fbx
│   ├── props/
│   │   ├── table.glb
│   │   └── table.glb.meta
│   └── environments/
│       └── warehouse.usd
├── animations/                # 动画文件 / Animation files
│   ├── walk_cycle.gltf
│   ├── walk_cycle.gltf.meta
│   └── idle.fbx
├── textures/                  # 独立纹理 / Standalone textures
│   ├── hdri/
│   │   ├── studio_01.exr
│   │   └── studio_01.exr.meta
│   └── materials/
│       └── metal_plate.png
├── audio/                     # 音频文件 / Audio files
│   ├── robot_sound.wav
│   └── robot_sound.wav.meta
├── urdf/                      # 机器人定义 / Robot definitions
│   ├── robot_a.urdf
│   └── robot_a.urdf.meta
├── scenes/                    # 场景定义文件 / Scene definition files
│   ├── main.rbcscene
│   ├── main.rbcscene.meta
│   ├── test_physics.rbcscene
│   └── environments/
│       └── warehouse.rbcscene
├── graphs/                    # 节点图定义 / Node graph definitions
│   ├── startup.rbcgraph
│   ├── startup.rbcgraph.meta
│   ├── animation_pipeline.rbcgraph
│   └── ai_inference/
│       └── text2model.rbcgraph
└── scripts/                   # 自定义脚本和节点 / Custom scripts and nodes
    ├── __init__.py
    ├── custom_nodes/
    │   ├── __init__.py
    │   ├── physics_nodes.py
    │   └── ai_nodes.py
    └── utils/
        ├── __init__.py
        └── data_processing.py
```

**支持的资产格式 / Supported Asset Formats:**

| 类型 | 格式 | 导入器 | 说明 |
|------|------|--------|------|
| 3D 模型 | `.gltf`, `.glb` | `GltfMeshImporter`, `GlbMeshImporter` | 推荐格式 |
| 3D 模型 | `.fbx` | `FbxMeshImporter` | 通过 FBX SDK |
| 3D 模型 | `.obj` | `ObjMeshImporter` | 简单静态模型 |
| 3D 模型 | `.usd`, `.usda`, `.usdc` | `UsdMeshImporter` | USD 格式 |
| 动画 | `.gltf`, `.glb` | `GltfAnimSequenceImporter` | 推荐格式 |
| 骨骼 | `.gltf`, `.glb` | `GltfSkeletonImporter` | 推荐格式 |
| 蒙皮 | `.gltf`, `.glb` | `GltfSkinImporter` | 推荐格式 |
| 纹理 | `.png`, `.jpg`, `.exr` | `ImageTextureImporter` | 通过 stb_image/tinyexr |
| 纹理 | `.tiff` | `TiffTextureImporter` | 通过 tinytiff |
| HDRI | `.exr`, `.hdr` | `HdriTextureImporter` | 环境贴图 |
| 机器人 | `.urdf` | `UrdfImporter` | 机器人定义 |

### 2.2 资产元数据 (`.meta` 文件) / Asset Metadata

**自动生成机制**:
- 当文件被复制到 `assets/` 目录时，系统会自动生成对应的 `.meta` 文件
- `.meta` 文件与资产文件同名，例如 `robot_a.gltf` → `robot_a.gltf.meta`
- `.meta` 文件包含 GUID、导入设置、依赖关系等信息

**`.meta` 文件格式 / Meta File Format:**

**示例 `robot_a.gltf.meta`:**

```json
{
  "guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "asset_type": "model",
  "import_settings": {
    "scale": 1.0,
    "up_axis": "Y",
    "generate_lightmap_uv": false,
    "import_materials": true,
    "import_animations": true,
    "import_skeleton": true,
    "import_skin": true,
    "texture_format": "auto"
  },
  "imported_at": "2025-12-31T12:00:00Z",
  "importer_version": "0.2.0",
  "file_hash": "sha256:a1b2c3d4...",
  "dependencies": [
    "assets/models/characters/robot_a_textures/"
  ]
}
```

**字段说明**:
- `guid`: 资产的唯一标识符，一旦生成不应改变
- `asset_type`: 资产类型提示（model, texture, scene, graph, script, etc.）
- `import_settings`: 导入器使用的设置参数
- `imported_at`: 最后导入时间
- `importer_version`: 导入器版本号
- `file_hash`: 文件内容哈希，用于检测变更
- `dependencies`: 依赖的其他资产路径

**重要特性**:
- ✅ **防止重复导入**: 通过 `file_hash` 检测文件是否变更
- ✅ **GUID 稳定性**: 同一文件的 GUID 保持不变
- ✅ **版本控制友好**: `.meta` 文件应提交到版本控制
- ✅ **导入设置持久化**: 团队成员使用相同的导入设置

---

## 3. 中间文件目录 / Intermediate Directory

### 3.1 `.rbc/` - 项目中间文件

存放所有生成的中间文件、缓存、日志等，**不应提交到版本控制**。

---

### 3.2 `.rbc/resources/` - 运行时资源

原始资产经过导入器处理后的二进制资源文件。

**设计理念**:
- **扁平结构**: 所有资源文件存储在同一目录，不按类型分子文件夹
- **`.rbcm` Meta 文件**: 每个资源有对应的 `.rbcm` 文件标识类型和元信息
- **Deprecate 追踪**: 支持标记过时资源，可通过工具清理

**目录结构 / Directory Structure:**

```
.rbc/resources/
├── resource_registry.db                           # 资源注册数据库（SQLite）
├── a1b2c3d4-e5f6-7890-abcd-ef1234567890.rbcb     # 资源数据文件（二进制）
├── a1b2c3d4-e5f6-7890-abcd-ef1234567890.rbcm     # 资源 Meta 文件（标识类型）
├── f1e2d3c4-b5a6-9788-1234-567890abcdef.rbcb     # 纹理资源
├── f1e2d3c4-b5a6-9788-1234-567890abcdef.rbcm
├── 12345678-abcd-ef12-3456-7890abcdef12.rbcb     # 材质资源
├── 12345678-abcd-ef12-3456-7890abcdef12.rbcm
├── 11223344-5566-7788-99aa-bbccddeeff00.rbcb     # 骨骼资源
├── 11223344-5566-7788-99aa-bbccddeeff00.rbcm
├── aabbccdd-eeff-0011-2233-445566778899.rbcb     # 蒙皮资源
├── aabbccdd-eeff-0011-2233-445566778899.rbcm
├── 99887766-5544-3322-1100-ffeeddccbbaa.rbcb     # 动画资源
├── 99887766-5544-3322-1100-ffeeddccbbaa.rbcm
└── bundles/                                       # 资源包目录
    └── character_pack_01.rbcbundle
```

**文件命名规则 / File Naming Rules:**

- **资源数据**: `{GUID}.rbcb` - 二进制资源数据
- **资源 Meta**: `{GUID}.rbcm` - JSON 格式的资源元信息
- **资源包**: `{bundle_name}.rbcbundle` - 打包的资源集合

**为什么使用扁平结构？**
1. ✅ **简化管理**: 不需要维护复杂的目录结构
2. ✅ **快速查找**: 通过 GUID 直接定位文件
3. ✅ **类型无关**: 添加新资源类型无需创建目录
4. ✅ **便于迁移**: 资源文件可以直接复制

**`.rbcb` 文件格式 / `.rbcb` File Format:**

RoboCute Binary Format，二进制序列化格式，存储资源的实际数据。

```
[Header: 32 bytes]
  - Magic: 'RBCB' (4 bytes)
  - Version: uint32 (4 bytes)
  - Resource Type: uint32 (4 bytes)
  - GUID: 128-bit (16 bytes)
  - Data Offset: uint64 (8 bytes)

[Meta Section: Variable]
  - Serialized metadata (JSON or binary)

[Data Section: Variable]
  - Actual resource data
```

**`.rbcm` 文件格式 / `.rbcm` File Format:**

RoboCute Meta Format，JSON 格式，存储资源的类型和元信息。

```json
{
  "guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "type": "mesh",
  "version": "1.0.0",
  "created_at": "2025-12-31T12:00:00Z",
  "modified_at": "2025-12-31T12:00:00Z",
  "source_asset": "assets/models/characters/robot_a.gltf",
  "file_size": 1048576,
  "deprecated": false,
  "deprecate_reason": "",
  "properties": {
    "vertex_count": 10240,
    "triangle_count": 5120,
    "submesh_count": 3,
    "has_skeleton": true,
    "has_skin": true
  },
  "dependencies": [
    "f1e2d3c4-b5a6-9788-1234-567890abcdef",
    "12345678-abcd-ef12-3456-7890abcdef12"
  ]
}
```

**`.rbcm` 字段说明**:
- `guid`: 资源的唯一标识符
- `type`: 资源类型（mesh, texture, material, skeleton, skin, animation, scene, graph, etc.）
- `version`: 资源版本号
- `source_asset`: 生成此资源的原始资产路径
- `file_size`: `.rbcb` 文件大小（字节）
- `deprecated`: 是否已废弃
- `deprecate_reason`: 废弃原因（如果 deprecated=true）
- `properties`: 资源特定的属性信息
- `dependencies`: 依赖的其他资源 GUID 列表

**`.rbcbundle` 文件格式 / `.rbcbundle` File Format:**

资源包文件，可以打包多个资源到一个文件中。

```
[Header: 64 bytes]
  - Magic: 'RBCPAK' (8 bytes)
  - Version: uint32 (4 bytes)
  - Resource Count: uint32 (4 bytes)
  - TOC Offset: uint64 (8 bytes)
  - TOC Size: uint64 (8 bytes)
  - ... (reserved)

[Resource Data Blocks]
  - Resource 1 data
  - Resource 2 data
  - ...

[Table of Contents (TOC)]
  - Entry 1: {GUID, Type, Offset, Size, Compression, ...}
  - Entry 2: ...
```

---

### 3.3 `.rbc/resources/resource_registry.db` - 资源注册数据库

SQLite 数据库，记录所有资源的元信息和依赖关系。

**数据库表结构 / Database Schema:**

```sql
-- 资源表
CREATE TABLE resources (
    guid TEXT PRIMARY KEY,
    type TEXT NOT NULL,              -- 'mesh', 'texture', 'material', 'scene', 'graph', etc.
    source_asset_path TEXT,          -- 原始资产路径（相对于 assets/）
    resource_file_path TEXT NOT NULL,-- .rbcb 文件路径
    file_offset INTEGER DEFAULT 0,   -- 文件内偏移（用于 bundle）
    size INTEGER NOT NULL,           -- 资源大小（字节）
    hash TEXT,                       -- 内容哈希（用于变更检测）
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    modified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    deprecated INTEGER DEFAULT 0,    -- 是否废弃（0=否，1=是）
    deprecate_reason TEXT,           -- 废弃原因
    import_settings TEXT,            -- JSON 格式的导入设置
    metadata TEXT                    -- 其他元数据（JSON）
);

-- 依赖关系表
CREATE TABLE dependencies (
    resource_guid TEXT NOT NULL,
    depends_on_guid TEXT NOT NULL,
    dependency_type TEXT,            -- 'texture', 'skeleton', 'material', etc.
    FOREIGN KEY (resource_guid) REFERENCES resources(guid),
    FOREIGN KEY (depends_on_guid) REFERENCES resources(guid),
    PRIMARY KEY (resource_guid, depends_on_guid)
);

-- 资产到资源的映射表
CREATE TABLE asset_to_resources (
    asset_path TEXT NOT NULL,
    resource_guid TEXT NOT NULL,
    FOREIGN KEY (resource_guid) REFERENCES resources(guid),
    PRIMARY KEY (asset_path, resource_guid)
);

-- 导入缓存表（用于检测资产变更）
CREATE TABLE import_cache (
    asset_path TEXT PRIMARY KEY,
    file_hash TEXT NOT NULL,
    file_size INTEGER NOT NULL,
    modified_time INTEGER NOT NULL,  -- Unix timestamp
    last_import_time INTEGER NOT NULL
);

-- 索引
CREATE INDEX idx_resources_type ON resources(type);
CREATE INDEX idx_resources_source ON resources(source_asset_path);
CREATE INDEX idx_resources_deprecated ON resources(deprecated);
CREATE INDEX idx_dependencies_resource ON dependencies(resource_guid);
CREATE INDEX idx_dependencies_depends ON dependencies(depends_on_guid);
```

**使用示例 / Usage Example:**

```sql
-- 查询一个资产导入后生成的所有资源
SELECT r.* FROM resources r
JOIN asset_to_resources a2r ON r.guid = a2r.resource_guid
WHERE a2r.asset_path = 'assets/models/characters/robot_a.gltf';

-- 查询一个资源的所有依赖
SELECT r.* FROM resources r
JOIN dependencies d ON r.guid = d.depends_on_guid
WHERE d.resource_guid = 'a1b2c3d4-e5f6-7890-abcd-ef1234567890';

-- 检查资产是否需要重新导入
SELECT * FROM import_cache
WHERE asset_path = 'assets/models/characters/robot_a.gltf'
  AND (file_hash != 'new_hash' OR file_size != new_size OR modified_time != new_mtime);

-- 查询所有废弃的资源
SELECT guid, type, source_asset_path, deprecate_reason, size
FROM resources
WHERE deprecated = 1
ORDER BY modified_at DESC;

-- 查询废弃资源占用的总空间
SELECT 
    type,
    COUNT(*) as count,
    SUM(size) as total_size
FROM resources
WHERE deprecated = 1
GROUP BY type;

-- 标记资源为废弃
UPDATE resources
SET deprecated = 1,
    deprecate_reason = 'Source asset deleted',
    modified_at = CURRENT_TIMESTAMP
WHERE guid = 'a1b2c3d4-e5f6-7890-abcd-ef1234567890';
```

---

### 3.4 `.rbc/cache/` - 缓存目录

存放各种临时缓存文件，可以安全删除。

```
.rbc/cache/
├── shaders/                   # 着色器编译缓存
│   ├── dx/
│   │   └── shader_hash.dxil
│   └── vk/
│       └── shader_hash.spv
├── thumbnails/                # 资源缩略图
│   └── a1b2c3d4-e5f6-7890-abcd-ef1234567890.png
└── temp/                      # 临时文件
    └── import_temp_12345.tmp
```

**缓存清理**:

所有缓存文件都可以安全删除，系统会在需要时重新生成。使用以下命令清理：

```bash
# 清理所有缓存
rbc cache clean --all

# 清理特定类型的缓存
rbc cache clean --shaders      # 清理着色器缓存
rbc cache clean --thumbnails   # 清理缩略图缓存
rbc cache clean --temp         # 清理临时文件

# 清理过期缓存（超过 N 天未使用）
rbc cache clean --older-than 30

# 清理超过指定大小的缓存
rbc cache clean --size-limit 1GB

# 显示缓存统计
rbc cache info
```

---

### 3.5 `.rbc/logs/` - 日志目录

存放日志文件和日志数据库。

```
.rbc/logs/
├── log.db                     # SQLite 日志数据库
├── app_2025_12_31.log         # 按日期分割的日志文件
└── crash_dumps/               # 崩溃转储文件
    └── crash_2025_12_31_120000.dmp
```

**日志数据库结构 / Log Database Schema:**

```sql
CREATE TABLE logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,      -- Unix timestamp (milliseconds)
    level TEXT NOT NULL,             -- 'DEBUG', 'INFO', 'WARNING', 'ERROR', 'FATAL'
    category TEXT,                   -- 'render', 'import', 'physics', etc.
    message TEXT NOT NULL,
    file TEXT,                       -- Source file
    line INTEGER,                    -- Line number
    function TEXT,                   -- Function name
    thread_id INTEGER,
    metadata TEXT                    -- JSON format
);

CREATE INDEX idx_logs_timestamp ON logs(timestamp);
CREATE INDEX idx_logs_level ON logs(level);
CREATE INDEX idx_logs_category ON logs(category);
```

---

### 3.6 `.rbc/out/` - 输出目录

默认的输出目录，用于渲染结果、导出文件等。

```
.rbc/out/
├── renders/                   # 渲染输出
│   ├── frame_0001.png
│   ├── frame_0002.png
│   └── animation.mp4
├── exports/                   # 导出的文件
│   └── robot_a_processed.gltf
└── datasets/                  # 生成的数据集
    └── training_data_001.json
```

---

## 4. 场景文件 / Scene Files

### 4.1 `assets/scenes/` - 场景目录

场景文件存放在 `assets/scenes/` 目录下，作为项目资产的一部分。

**目录结构 / Directory Structure:**

```
assets/scenes/
├── main.rbcscene
├── main.rbcscene.meta             # 自动生成的 meta 文件
├── test_physics.rbcscene
├── test_physics.rbcscene.meta
└── environments/
    ├── warehouse.rbcscene
    └── warehouse.rbcscene.meta
```

**场景文件格式 `.rbcscene` / Scene File Format:**

场景文件使用 RoboCute 的二进制序列化格式，包含所有 Entity、Component 和资源引用。

**文件结构 / File Structure:**

```
[Header: 32 bytes]
  - Magic: 'RBCS' (4 bytes)
  - Version: uint32 (4 bytes)
  - Entity Count: uint32 (4 bytes)
  - ... (reserved)

[Entity Serialization]
  - Entity 1
    - GUID
    - Components
      - Component 1 (Type ID + Serialized Data)
      - Component 2
      - ...
  - Entity 2
  - ...

[Resource References]
  - Resource GUID 1
  - Resource GUID 2
  - ...
```

**场景元数据文件 `.rbcscene.meta`:**

```json
{
  "name": "Main Scene",
  "description": "The main simulation scene",
  "created_at": "2025-12-31T00:00:00Z",
  "modified_at": "2025-12-31T12:00:00Z",
  "entity_count": 42,
  "referenced_resources": [
    "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
    "f1e2d3c4-b5a6-9788-1234-567890abcdef"
  ],
  "tags": ["main", "simulation"]
}
```

---

## 5. 节点图文件 / Node Graph Files

### 5.1 `assets/graphs/` - 节点图目录

节点图文件存放在 `assets/graphs/` 目录下，作为项目资产的一部分。

**目录结构 / Directory Structure:**

```
assets/graphs/
├── startup.rbcgraph
├── startup.rbcgraph.meta          # 自动生成的 meta 文件
├── animation_pipeline.rbcgraph
├── animation_pipeline.rbcgraph.meta
└── ai_inference/
    ├── text2model.rbcgraph
    └── text2model.rbcgraph.meta
```

**节点图文件格式 `.rbcgraph` / Node Graph File Format:**

节点图使用 JSON 格式存储（考虑到可读性和版本控制友好）。

**文件格式 / File Format:**

```json
{
  "name": "Animation Pipeline",
  "description": "Character animation processing pipeline",
  "version": "0.1.0",
  "created_at": "2025-12-31T00:00:00Z",
  "modified_at": "2025-12-31T12:00:00Z",
  
  "variables": {
    "frame_rate": 30.0,
    "duration": 10.0
  },
  
  "nodes": [
    {
      "id": "node_001",
      "type": "entity_input",
      "position": [100, 100],
      "inputs": {
        "entity_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
      },
      "metadata": {
        "color": "#FF5733",
        "collapsed": false
      }
    },
    {
      "id": "node_002",
      "type": "rotation_animation",
      "position": [300, 100],
      "inputs": {
        "radius": 2.0,
        "angular_velocity": 1.0,
        "duration_frames": 300,
        "fps": "$frame_rate"
      }
    },
    {
      "id": "node_003",
      "type": "animation_output",
      "position": [500, 100],
      "inputs": {
        "name": "rotation_test",
        "fps": "$frame_rate"
      }
    }
  ],
  
  "connections": [
    {
      "from_node": "node_001",
      "from_output": "entity",
      "to_node": "node_002",
      "to_input": "entity"
    },
    {
      "from_node": "node_002",
      "from_output": "animation",
      "to_node": "node_003",
      "to_input": "animation"
    }
  ],
  
  "groups": [
    {
      "name": "Animation Generation",
      "nodes": ["node_001", "node_002"],
      "color": "#3498db"
    }
  ]
}
```

---

## 6. 脚本和自定义节点 / Scripts and Custom Nodes

### 6.1 `assets/scripts/` - 脚本目录

脚本文件存放在 `assets/scripts/` 目录下，作为项目资产的一部分。

**目录结构 / Directory Structure:**

```
assets/scripts/
├── __init__.py
├── custom_nodes/
│   ├── __init__.py
│   ├── physics_nodes.py
│   ├── physics_nodes.py.meta   # Python 文件也有 meta（可选）
│   ├── ai_nodes.py
│   └── ai_nodes.py.meta
├── utils/
│   ├── __init__.py
│   └── data_processing.py
└── startup.py                  # 启动脚本
```

**说明**:
- Python 脚本也可以有 `.meta` 文件，用于追踪版本和依赖
- 脚本修改后，相关的节点图会自动检测到变更
- 支持脚本的热重载（开发模式）

**自定义节点示例 `custom_nodes/physics_nodes.py`:**

```python
import robocute as rbc
from robocute.node_graph import BaseNode, NodeInput, NodeOutput

class ApplyForceNode(BaseNode):
    """Apply force to a rigidbody"""
    
    node_type = "apply_force"
    
    inputs = {
        "entity": NodeInput(type=rbc.Entity, required=True),
        "force": NodeInput(type=list, default=[0.0, 10.0, 0.0]),
        "point": NodeInput(type=list, default=[0.0, 0.0, 0.0]),
    }
    
    outputs = {
        "entity": NodeOutput(type=rbc.Entity),
    }
    
    def execute(self, context):
        entity = self.get_input("entity")
        force = self.get_input("force")
        point = self.get_input("point")
        
        # Apply force logic
        rigidbody = entity.get_component(rbc.RigidbodyComponent)
        if rigidbody:
            rigidbody.apply_force(force, point)
        
        return {"entity": entity}

# Register node
rbc.register_node(ApplyForceNode)
```

---

## 7. 数据集和预训练模型 / Datasets and Pretrained Models

### 7.1 `datasets/` - 数据集目录

存放训练数据集。

```
datasets/
├── robot_motion/
│   ├── train/
│   │   ├── sequence_001.json
│   │   └── sequence_002.json
│   ├── val/
│   └── dataset_info.json
└── text_to_3d/
    └── prompts.jsonl
```

### 7.2 `pretrained/` - 预训练模型目录

存放预训练模型权重。

```
pretrained/
├── text2image/
│   └── stable_diffusion_v2.safetensors
├── text2model/
│   └── shap_e_weights.pt
└── models_info.json
```

**`models_info.json` 示例:**

```json
{
  "models": [
    {
      "name": "stable_diffusion_v2",
      "type": "text2image",
      "path": "text2image/stable_diffusion_v2.safetensors",
      "size": 4265615360,
      "hash": "sha256:a1b2c3d4...",
      "source": "https://huggingface.co/...",
      "license": "CreativeML Open RAIL-M"
    }
  ]
}
```

---

## 8. 版本控制建议 / Version Control Recommendations

### 8.1 应该提交的文件 / Files to Commit

```
rbc_project.json
.rbcignore
README.md
assets/           # 包括所有资产：models, scenes, graphs, scripts, etc.
  *.meta          # 包括所有 .meta 文件（重要！）
docs/
datasets/         # 如果数据集不太大
pretrained/       # 考虑使用 Git LFS
```

### 8.2 不应该提交的文件 / Files NOT to Commit

```
.rbc/             # 所有中间文件
*.log
*.tmp
*.cache
.DS_Store
Thumbs.db
```

### 8.3 Git LFS 建议 / Git LFS Recommendations

对于大文件，建议使用 Git LFS：

```gitattributes
# .gitattributes
*.gltf filter=lfs diff=lfs merge=lfs -text
*.glb filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.usd filter=lfs diff=lfs merge=lfs -text
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
*.hdr filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
*.mp4 filter=lfs diff=lfs merge=lfs -text
*.safetensors filter=lfs diff=lfs merge=lfs -text
*.pt filter=lfs diff=lfs merge=lfs -text
*.onnx filter=lfs diff=lfs merge=lfs -text
```

---

## 9. 资源导入流程 / Resource Import Pipeline

### 9.1 导入流程图 / Import Pipeline Flow

```
Original Asset (.gltf/.fbx/etc.)
         ↓
    [Detect Change]
    (check import_cache)
         ↓
    [Resource Importer]
    (GltfMeshImporter, etc.)
         ↓
    [Generate Resources]
    (Mesh, Texture, Skeleton, etc.)
         ↓
    [Assign GUID]
    (if not exists in .meta)
         ↓
    [Serialize to .rbcb]
    (.rbc/resources/{type}/{guid}.rbcb)
         ↓
    [Update Registry DB]
    (resource_registry.db)
         ↓
    [Update Import Cache]
    (import_cache table)
```

### 9.2 资源加载流程 / Resource Loading Flow

```
Request Resource by GUID
         ↓
    [Check Resource Status]
    (Unloaded/Loading/Loaded)
         ↓
    [Query Registry DB]
    (get resource_file_path, file_offset)
         ↓
    [Load .rbcb File]
    (read binary data)
         ↓
    [Deserialize Resource]
    (Resource::deserialize_meta)
         ↓
    [Initialize Device Resource]
    (upload to GPU if needed)
         ↓
    [Set Status to Loaded]
```

---

## 10. 资源打包和分发 / Resource Packaging and Distribution

### 10.1 资源打包工具 / Resource Packer Tool

RoboCute 提供资源打包工具，可以将多个 `.rbcb` 文件打包成一个 `.rbcbundle`。

**命令行工具:**

```bash
# 打包资源（指定资源 GUID 列表）
rbc pack --input-guids a1b2c3d4-xxx,f1e2d3c4-xxx \
         --output character_pack.rbcbundle \
         --compress zstd \
         --level 3

# 打包资源（按类型）
rbc pack --type mesh,texture \
         --output assets_pack.rbcbundle

# 解包资源
rbc unpack --input character_pack.rbcbundle \
           --output .rbc/resources/

# 列出包内容
rbc pack list --input character_pack.rbcbundle

# 清理废弃资源
rbc resources clean-deprecated --dry-run   # 预览将要删除的资源
rbc resources clean-deprecated             # 实际删除

# 标记资源为废弃
rbc resources deprecate --guid a1b2c3d4-xxx \
                        --reason "Replaced by new version"

# 查看废弃资源
rbc resources list-deprecated
```

**Python API:**

```python
import robocute as rbc

# Create a resource bundle
bundle = rbc.ResourceBundle("character_pack.rbcbundle")
bundle.add_resource("a1b2c3d4-e5f6-7890-abcd-ef1234567890")
bundle.add_resource("f1e2d3c4-b5a6-9788-1234-567890abcdef")
bundle.save(compression="zstd", level=3)

# Load resources from bundle
bundle = rbc.ResourceBundle.load("character_pack.rbcbundle")
mesh = bundle.load_resource("a1b2c3d4-e5f6-7890-abcd-ef1234567890")
```

### 10.2 项目导出 / Project Export

导出完整项目用于分发：

```bash
# 导出项目（包含资源）
rbc export-project --project MyRoboCuteProject \
                   --output MyProject_v0.1.0.rbcproj \
                   --include-assets \
                   --compress

# 导入项目
rbc import-project --input MyProject_v0.1.0.rbcproj \
                   --destination ~/RoboCuteProjects/
```

---

## 11. 应用级配置 / Application-Level Configuration

应用级配置存储在用户目录下，与项目无关。

### 11.1 配置文件位置 / Configuration File Location

**Windows:**
```
%APPDATA%/RoboCute/config.json
```

**Linux/macOS:**
```
~/.config/robocute/config.json
```

### 11.2 配置文件格式 / Configuration File Format

```json
{
  "version": "0.2.0",
  "runtime": {
    "executable_path": "C:/Program Files/RoboCute/bin/rbc_editor.exe",
    "shader_search_paths": [
      "C:/Program Files/RoboCute/shaders/",
      "~/.robocute/custom_shaders/"
    ]
  },
  "editor": {
    "theme": "dark",
    "font_size": 12,
    "auto_save_interval": 300,
    "recent_projects": [
      "D:/Projects/MyRoboCuteProject",
      "D:/Projects/RobotSimulation"
    ],
    "default_project_path": "D:/Projects/"
  },
  "rendering": {
    "default_backend": "dx",
    "vsync": true,
    "max_fps": 60
  },
  "python": {
    "interpreter_path": "C:/Python313/python.exe",
    "site_packages": [
      "~/.robocute/site-packages/"
    ]
  }
}
```

---

## 12. 最佳实践 / Best Practices

### 12.1 项目组织 / Project Organization

1. **保持原始资产整洁**: 按类型和功能组织 `assets/` 目录
2. **使用有意义的命名**: 避免使用 `model_001.gltf` 这样的名字
3. **记录导入设置**: 在 `.meta` 文件中记录导入参数
4. **定期清理缓存**: 删除 `.rbc/cache/` 中的过期文件

### 12.2 版本控制 / Version Control

1. **永远不要提交 `.rbc/` 目录**
2. **使用 Git LFS 管理大文件**
3. **提交 `.meta` 文件**: 保持导入设置的一致性
4. **使用语义化版本**: 在 `rbc_project.json` 中使用语义化版本号

### 12.3 资源管理 / Resource Management

1. **重用资源**: 通过 GUID 引用资源，避免重复导入
2. **管理依赖**: 确保资源依赖关系在数据库中正确记录
3. **定期备份**: 备份项目文件和资源数据库
4. **使用资源包**: 对于发布版本，打包资源以减少文件数量

### 12.4 性能优化 / Performance Optimization

1. **资源流式加载**: 使用异步加载避免阻塞
2. **纹理压缩**: 使用适当的纹理格式和压缩
3. **LOD 管理**: 为大模型准备多级细节
4. **缓存着色器**: 利用 `.rbc/cache/shaders/` 加速启动

---

## 13. 迁移和升级 / Migration and Upgrade

### 13.1 版本升级 / Version Upgrade

当 RoboCute 版本升级时，可能需要迁移项目：

```bash
# 检查项目兼容性
rbc check-project --project MyRoboCuteProject

# 升级项目到新版本
rbc migrate-project --project MyRoboCuteProject \
                    --from 0.1.0 \
                    --to 0.2.0 \
                    --backup
```

### 13.2 资源重新导入 / Resource Reimport

当资源格式变更时：

```bash
# 重新导入所有资源
rbc reimport-all --project MyRoboCuteProject

# 重新导入特定类型
rbc reimport --type mesh --project MyRoboCuteProject
```

---

## 附录 A: 完整示例项目 / Appendix A: Complete Example Project

参考示例项目结构：

```
MyRobotProject/
├── rbc_project.json
├── .rbcignore
├── .gitignore
├── .gitattributes
├── README.md
├── assets/                                      # 所有资产统一管理
│   ├── models/
│   │   ├── robot_chassis.glb
│   │   └── robot_chassis.glb.meta
│   ├── textures/
│   │   ├── metal_texture.png
│   │   └── metal_texture.png.meta
│   ├── scenes/
│   │   ├── main.rbcscene
│   │   └── main.rbcscene.meta
│   ├── graphs/
│   │   ├── physics_sim.rbcgraph
│   │   └── physics_sim.rbcgraph.meta
│   └── scripts/
│       ├── __init__.py
│       └── custom_nodes/
│           ├── __init__.py
│           └── robot_control.py
├── docs/
│   └── project_notes.md
└── .rbc/
    ├── resources/                               # 扁平结构，使用 .rbcm 标识类型
    │   ├── resource_registry.db
    │   ├── a1b2c3d4-xxx.rbcb
    │   ├── a1b2c3d4-xxx.rbcm
    │   ├── f1e2d3c4-xxx.rbcb
    │   └── f1e2d3c4-xxx.rbcm
    ├── cache/
    ├── logs/
    └── out/
```

---

## 附录 B: 资源 GUID 生成规则 / Appendix B: Resource GUID Generation Rules

GUID 生成规则：

1. **确定性生成**: 对于相同的资产文件和导入设置，应该生成相同的 GUID
2. **格式**: 使用标准的 UUID v5（基于 SHA-1 哈希）
3. **命名空间**: 使用 RoboCute 专用的命名空间 UUID

```python
import uuid
import hashlib

# RoboCute namespace UUID
RBC_NAMESPACE = uuid.UUID('6ba7b810-9dad-11d1-80b4-00c04fd430c8')

def generate_resource_guid(asset_path: str, import_settings: dict) -> uuid.UUID:
    """Generate deterministic GUID for a resource"""
    # Combine asset path and import settings
    data = f"{asset_path}|{json.dumps(import_settings, sort_keys=True)}"
    return uuid.uuid5(RBC_NAMESPACE, data)
```

---

## 总结 / Summary

本文档定义了 RoboCute 项目的完整文件结构规范，涵盖：

- 项目配置文件格式
- 原始资产管理
- 运行时资源存储
- 资源注册数据库
- 场景和节点图文件格式
- 资源导入和加载流程
- 资源打包和分发
- 应用级配置
- 最佳实践建议

遵循此规范可以确保项目结构清晰、资源管理高效、版本控制友好，并且易于团队协作和项目分发。

---

**文档版本**: v1.0.0  
**最后更新**: 2025-12-31  
**作者**: RoboCute Team

