# Project System Implementation Checklist

本文档提供了 RoboCute 项目系统实现的详细任务清单。

This document provides a detailed task checklist for implementing the RoboCute project system.

---

## Phase 1: 资源导入器框架 / Resource Importer Framework

### Task 1.1: 资源导入器接口完善 ✅ (Partial)

**文件**: `rbc/runtime/include/rbc_world/resource_importer.h`

- [x] `IResourceImporter` 基类接口
- [x] `IMeshImporter` 接口
- [x] `ITextureImporter` 接口
- [ ] `IMaterialImporter` 接口
- [ ] `ISkeletonImporter` 接口（已存在但需要验证）
- [ ] `ISkinImporter` 接口（已存在但需要验证）
- [ ] `IAnimSequenceImporter` 接口（已存在但需要验证）

**参考**: [resource_management.md](resource_management.md)

### Task 1.2: 资源导入器注册表实现 ✅ (Partial)

**文件**: `rbc/runtime/src/world/importers/resource_importer_registry.cpp`

- [x] `ResourceImporterRegistry` 基本实现
- [ ] 线程安全保证（验证 `_mtx` 使用）
- [ ] 扩展名规范化函数
- [ ] 单元测试

**测试文件**: `rbc/tests/test_resource_importer_registry.cpp`

```cpp
// 测试用例
TEST_CASE("ResourceImporterRegistry") {
    auto& registry = ResourceImporterRegistry::instance();
    
    SUBCASE("Register and find importer") {
        GltfMeshImporter importer;
        registry.register_importer(&importer);
        
        auto found = registry.find_importer(".gltf", ResourceType::Mesh);
        CHECK(found != nullptr);
        CHECK(found->extension() == ".gltf");
    }
    
    SUBCASE("Unregister importer") {
        // Test unregistration
    }
    
    SUBCASE("Multiple importers for same extension") {
        // Test priority handling
    }
}
```

### Task 1.3: 注册内置导入器 🚧

**文件**: `rbc/runtime/src/world/importers/register_importers.cpp`

- [x] GLTF Mesh Importer 注册
- [x] GLTF Skeleton Importer 注册
- [x] GLTF Skin Importer 注册
- [x] GLTF Animation Importer 注册
- [ ] Texture Importer 注册（PNG, JPG, EXR, TIFF）
- [ ] Material Importer 注册
- [ ] FBX Importer 注册（可选）
- [ ] OBJ Importer 注册

**实现示例**:

```cpp
namespace rbc::world {

void register_builtin_importers() {
    auto& registry = ResourceImporterRegistry::instance();
    
    // Mesh importers
    static GltfMeshImporter gltf_mesh_importer;
    static GlbMeshImporter glb_mesh_importer;
    registry.register_importer(&gltf_mesh_importer);
    registry.register_importer(&glb_mesh_importer);
    
    // Texture importers
    static PngTextureImporter png_importer;
    static JpgTextureImporter jpg_importer;
    static ExrTextureImporter exr_importer;
    registry.register_importer(&png_importer);
    registry.register_importer(&jpg_importer);
    registry.register_importer(&exr_importer);
    
    // TODO: Add more importers
}

} // namespace rbc::world
```

---

## Phase 2: 资源注册数据库 / Resource Registry Database

### Task 2.1: SQLite 数据库包装类 🔜

**文件**: `rbc/runtime/include/rbc_world/resource_registry_db.h`

- [ ] `ResourceRegistryDB` 类定义
- [ ] `ResourceInfo` 结构体
- [ ] `ImportCacheEntry` 结构体
- [ ] 数据库连接管理
- [ ] 错误处理

**依赖**: 添加 SQLite 库到项目

**CMakeLists.txt**:
```cmake
find_package(SQLite3 REQUIRED)
target_link_libraries(rbc_runtime PRIVATE SQLite::SQLite3)
```

**xmake.lua**:
```lua
add_requires("sqlite3")
target("rbc_runtime")
    add_packages("sqlite3")
```

### Task 2.2: 数据库表结构初始化 🔜

**文件**: `rbc/runtime/src/world/resource_registry_db.cpp`

```cpp
void ResourceRegistryDB::Impl::initialize_schema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS resources (
            guid TEXT PRIMARY KEY,
            type TEXT NOT NULL,
            source_asset_path TEXT,
            resource_file_path TEXT NOT NULL,
            file_offset INTEGER DEFAULT 0,
            size INTEGER NOT NULL,
            hash TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now')),
            modified_at INTEGER DEFAULT (strftime('%s', 'now')),
            import_settings TEXT,
            metadata TEXT
        );
        
        CREATE TABLE IF NOT EXISTS dependencies (
            resource_guid TEXT NOT NULL,
            depends_on_guid TEXT NOT NULL,
            dependency_type TEXT,
            FOREIGN KEY (resource_guid) REFERENCES resources(guid),
            FOREIGN KEY (depends_on_guid) REFERENCES resources(guid),
            PRIMARY KEY (resource_guid, depends_on_guid)
        );
        
        CREATE TABLE IF NOT EXISTS asset_to_resources (
            asset_path TEXT NOT NULL,
            resource_guid TEXT NOT NULL,
            FOREIGN KEY (resource_guid) REFERENCES resources(guid),
            PRIMARY KEY (asset_path, resource_guid)
        );
        
        CREATE TABLE IF NOT EXISTS import_cache (
            asset_path TEXT PRIMARY KEY,
            file_hash TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            modified_time INTEGER NOT NULL,
            last_import_time INTEGER NOT NULL
        );
        
        CREATE INDEX IF NOT EXISTS idx_resources_type ON resources(type);
        CREATE INDEX IF NOT EXISTS idx_resources_source ON resources(source_asset_path);
        CREATE INDEX IF NOT EXISTS idx_dependencies_resource ON dependencies(resource_guid);
        CREATE INDEX IF NOT EXISTS idx_dependencies_depends ON dependencies(depends_on_guid);
    )";
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, schema, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        LUISA_ERROR("Failed to create schema: {}", err_msg);
        sqlite3_free(err_msg);
    }
}
```

**任务**:
- [ ] 实现 `initialize_schema()`
- [ ] 实现 `add_resource()`
- [ ] 实现 `update_resource()`
- [ ] 实现 `remove_resource()`
- [ ] 实现 `get_resource()`
- [ ] 实现 `add_dependency()`
- [ ] 实现 `get_dependencies()`
- [ ] 实现 `add_asset_mapping()`
- [ ] 实现 `get_resources_from_asset()`
- [ ] 实现 `update_import_cache()`
- [ ] 实现 `needs_reimport()`

### Task 2.3: 数据库查询和事务 🔜

**任务**:
- [ ] 实现预编译语句（Prepared Statements）
- [ ] 实现事务支持（BEGIN/COMMIT/ROLLBACK）
- [ ] 实现批量操作优化
- [ ] 实现查询结果缓存（可选）

**示例**:
```cpp
bool ResourceRegistryDB::add_resources_batch(
    luisa::span<const ResourceInfo> resources) {
    
    // Begin transaction
    sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    
    for (auto const& info : resources) {
        if (!add_resource(info)) {
            // Rollback on error
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
    }
    
    // Commit transaction
    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    return true;
}
```

---

## Phase 3: 资源导入管理器 / Resource Import Manager

### Task 3.1: 资源导入管理器接口 🔜

**文件**: `rbc/runtime/include/rbc_world/resource_import_manager.h`

- [ ] `ImportOptions` 结构体
- [ ] `ImportResult` 结构体
- [ ] `ResourceImportManager` 类定义
- [ ] `import_asset()` 方法
- [ ] `import_assets()` 方法
- [ ] `import_directory()` 方法
- [ ] `reimport_all()` 方法
- [ ] `needs_reimport()` 方法
- [ ] `get_import_stats()` 方法

### Task 3.2: 资源导入核心逻辑 🔜

**文件**: `rbc/runtime/src/world/resource_import_manager.cpp`

**实现步骤**:

1. **检查导入缓存**
```cpp
if (!options.force_reimport && !registry_db->needs_reimport(asset_path.string())) {
    // Skip import, return cached resources
    result.imported_resources = registry_db->get_resources_from_asset(asset_path.string());
    return result;
}
```

2. **加载或生成 .meta 文件**
```cpp
auto meta_path = luisa::filesystem::path(asset_path.string() + ".meta");
AssetMetadata metadata;
if (luisa::filesystem::exists(meta_path)) {
    metadata = load_asset_metadata(meta_path);
} else {
    metadata.guid = vstd::Guid::create();
    metadata.import_settings = options.import_settings_json;
    save_asset_metadata(meta_path, metadata);
}
```

3. **查找并执行导入器**
```cpp
auto extension = asset_path.extension().string();
auto importer = ResourceImporterRegistry::instance().find_importer(extension, type);
if (importer && importer->can_import(asset_path)) {
    auto resource = create_resource_for_type(type, metadata.guid);
    importer->import(resource, asset_path);
}
```

4. **序列化到 .rbcb 文件**
```cpp
auto resource_filename = fmt::format("{}.rbcb", resource->guid().to_string());
auto resource_path = resources_dir / resource_type_to_string(type) / resource_filename;
resource->set_path(resource_path, 0);
resource->save_to_path();
```

5. **更新数据库**
```cpp
ResourceInfo info;
info.guid = resource->guid();
info.type = resource_type_to_string(type);
info.source_asset_path = asset_path.string();
info.resource_file_path = resource_path.string();
registry_db->add_resource(info);
registry_db->add_asset_mapping(asset_path.string(), resource->guid());
```

6. **更新导入缓存**
```cpp
ImportCacheEntry cache_entry;
cache_entry.asset_path = asset_path.string();
cache_entry.file_size = luisa::filesystem::file_size(asset_path);
cache_entry.modified_time = /* file mtime */;
cache_entry.last_import_time = /* current time */;
registry_db->update_import_cache(asset_path.string(), cache_entry);
```

**任务清单**:
- [ ] 实现 `import_single_asset()`
- [ ] 实现 `import_directory()` (递归遍历)
- [ ] 实现 `reimport_all()`
- [ ] 实现文件哈希计算（用于变更检测）
- [ ] 实现进度报告（可选）
- [ ] 实现错误恢复和回滚

### Task 3.3: 资源元数据管理 🔜

**文件**: `rbc/runtime/include/rbc_world/asset_metadata.h`

```cpp
namespace rbc::world {

struct AssetMetadata {
    vstd::Guid guid;
    luisa::string import_settings;  // JSON
    uint64_t imported_at;
    luisa::string importer_version;
    luisa::vector<luisa::string> dependencies;
};

AssetMetadata load_asset_metadata(luisa::filesystem::path const& meta_path);
void save_asset_metadata(luisa::filesystem::path const& meta_path, AssetMetadata const& metadata);

} // namespace rbc::world
```

**实现**:
- [ ] JSON 序列化/反序列化
- [ ] GUID 生成规则
- [ ] 依赖关系记录

---

## Phase 4: 项目管理系统 / Project Management System

### Task 4.1: 项目配置结构 🔜

**文件**: `rbc/runtime/include/rbc_project/project_config.h`

```cpp
namespace rbc::project {

struct ProjectConfig {
    luisa::string name;
    luisa::string version = "0.1.0";
    luisa::string rbc_version;
    luisa::string author;
    luisa::string description;
    luisa::string license = "Apache-2.0";
    
    luisa::unordered_map<luisa::string, luisa::string> paths;
    luisa::unordered_map<luisa::string, luisa::string> config;
    luisa::unordered_map<luisa::string, luisa::string> metadata;
};

// JSON 序列化
void to_json(nlohmann::json& j, ProjectConfig const& config);
void from_json(nlohmann::json const& j, ProjectConfig& config);

} // namespace rbc::project
```

**任务**:
- [ ] 定义 `ProjectConfig` 结构体
- [ ] 实现 JSON 序列化
- [ ] 实现 JSON 反序列化
- [ ] 验证配置有效性

### Task 4.2: Project 类实现 🔜

**文件**: `rbc/runtime/include/rbc_project/project.h`

**任务**:
- [ ] `Project::create()` - 创建新项目
- [ ] `Project::load()` - 加载现有项目
- [ ] `Project::save()` - 保存项目配置
- [ ] 路径访问方法（`root_path()`, `assets_path()`, etc.）
- [ ] 资源管理器访问（`import_manager()`, `resource_registry()`）
- [ ] 场景管理（`load_default_scene()`, `save_current_scene()`）

### Task 4.3: 项目目录初始化 🔜

**文件**: `rbc/runtime/src/project/project_init.cpp`

```cpp
bool Project::create_directory_structure() {
    std::error_code ec;
    
    // Create main directories
    for (auto const& [key, rel_path] : _impl->config.paths) {
        auto dir_path = _impl->root_path / rel_path;
        luisa::filesystem::create_directories(dir_path, ec);
        if (ec) {
            LUISA_ERROR("Failed to create directory {}: {}", 
                       dir_path.string(), ec.message());
            return false;
        }
    }
    
    // Create .rbc/ subdirectories
    auto rbc_dir = intermediate_path();
    create_subdirectories(rbc_dir, {
        "resources/meshes",
        "resources/textures",
        "resources/materials",
        "resources/skeletons",
        "resources/skins",
        "resources/animations",
        "cache/shaders/dx",
        "cache/shaders/vk",
        "cache/thumbnails",
        "cache/temp",
        "logs",
        "out/renders",
        "out/exports",
        "out/datasets"
    });
    
    return true;
}
```

**任务**:
- [ ] 实现 `create_directory_structure()`
- [ ] 实现 `create_default_files()`
  - [ ] `.rbcignore`
  - [ ] `.gitignore`
  - [ ] `.gitattributes`
  - [ ] `README.md`
  - [ ] `main.py`
- [ ] 实现 `initialize_databases()`

### Task 4.4: 项目文件辅助函数 🔜

**文件**: `rbc/runtime/src/project/project_helpers.cpp`

**任务**:
- [ ] `create_rbcignore_file()`
- [ ] `create_gitignore_file()`
- [ ] `create_gitattributes_file()`
- [ ] `create_readme_file()`
- [ ] `create_default_main_py()`
- [ ] `init_git_repo()` (调用 git 命令)

---

## Phase 5: Python Bindings

### Task 5.1: ResourceImportManager Python Bindings 🔜

**文件**: `rbc/ext_c/src/bindings/resource_bindings.cpp`

```cpp
#include <pybind11/pybind11.h>
#include <rbc_world/resource_import_manager.h>

namespace py = pybind11;

void bind_resource_import_manager(py::module& m) {
    py::class_<rbc::world::ImportOptions>(m, "ImportOptions")
        .def(py::init<>())
        .def_readwrite("force_reimport", &rbc::world::ImportOptions::force_reimport)
        .def_readwrite("async_import", &rbc::world::ImportOptions::async_import)
        .def_readwrite("generate_thumbnails", &rbc::world::ImportOptions::generate_thumbnails)
        .def_readwrite("import_settings_json", &rbc::world::ImportOptions::import_settings_json);
    
    py::class_<rbc::world::ImportResult>(m, "ImportResult")
        .def_readonly("success", &rbc::world::ImportResult::success)
        .def_readonly("imported_resources", &rbc::world::ImportResult::imported_resources)
        .def_readonly("error_message", &rbc::world::ImportResult::error_message)
        .def_readonly("import_time_seconds", &rbc::world::ImportResult::import_time_seconds);
    
    py::class_<rbc::world::ResourceImportManager>(m, "ResourceImportManager")
        .def(py::init<luisa::filesystem::path const&, luisa::filesystem::path const&>())
        .def("import_asset", &rbc::world::ResourceImportManager::import_asset)
        .def("import_directory", &rbc::world::ResourceImportManager::import_directory)
        .def("needs_reimport", &rbc::world::ResourceImportManager::needs_reimport)
        .def("get_resources_from_asset", &rbc::world::ResourceImportManager::get_resources_from_asset);
}
```

**任务**:
- [ ] 实现 `bind_resource_import_manager()`
- [ ] 实现 `bind_resource_registry_db()`
- [ ] 添加到主绑定模块

### Task 5.2: Project Python Bindings 🔜

**文件**: `rbc/ext_c/src/bindings/project_bindings.cpp`

```cpp
void bind_project(py::module& m) {
    py::class_<rbc::project::ProjectConfig>(m, "ProjectConfig")
        .def(py::init<>())
        .def_readwrite("name", &rbc::project::ProjectConfig::name)
        .def_readwrite("version", &rbc::project::ProjectConfig::version)
        .def_readwrite("author", &rbc::project::ProjectConfig::author)
        .def_readwrite("description", &rbc::project::ProjectConfig::description);
    
    py::class_<rbc::project::Project>(m, "Project")
        .def_static("create", &rbc::project::Project::create)
        .def_static("load", &rbc::project::Project::load)
        .def("save", &rbc::project::Project::save)
        .def("root_path", &rbc::project::Project::root_path)
        .def("assets_path", &rbc::project::Project::assets_path)
        .def("scenes_path", &rbc::project::Project::scenes_path)
        .def("import_manager", &rbc::project::Project::import_manager, 
             py::return_value_policy::reference);
}
```

**任务**:
- [ ] 实现 `bind_project()`
- [ ] 添加到主绑定模块

### Task 5.3: Python 包装层 🔜

**文件**: `src/robocute/project.py`

```python
from typing import Optional
from pathlib import Path
import robocute_ext as _rbc_ext

class Project:
    """RoboCute project manager"""
    
    def __init__(self, impl):
        self._impl = impl
    
    @staticmethod
    def create(path: str, name: str, **kwargs) -> 'Project':
        """Create a new project"""
        config = {'name': name, **kwargs}
        impl = _rbc_ext.Project.create(path, config)
        return Project(impl)
    
    @staticmethod
    def load(path: str) -> 'Project':
        """Load existing project"""
        impl = _rbc_ext.Project.load(path)
        return Project(impl)
    
    def import_asset(self, asset_path: str, **kwargs):
        """Import an asset"""
        importer = self._impl.import_manager()
        options = _rbc_ext.ImportOptions()
        options.force_reimport = kwargs.get('force_reimport', False)
        return importer.import_asset(asset_path, options)
```

**任务**:
- [ ] 实现 `Project` 包装类
- [ ] 实现 `ResourceImportManager` 包装
- [ ] 添加类型提示
- [ ] 编写文档字符串

---

## Phase 6: CLI 工具 / Command Line Tools

### Task 6.1: Project CLI 🔜

**文件**: `src/robocute/cli/project.py`

**命令**:
- [ ] `rbc project create` - 创建新项目
- [ ] `rbc project info` - 显示项目信息
- [ ] `rbc project validate` - 验证项目结构
- [ ] `rbc project list` - 列出最近项目

### Task 6.2: Import CLI 🔜

**文件**: `src/robocute/cli/import_assets.py`

**命令**:
- [ ] `rbc import asset` - 导入单个资产
- [ ] `rbc import dir` - 导入目录
- [ ] `rbc import list` - 列出已导入资源
- [ ] `rbc import clean` - 清理未使用资源

### Task 6.3: Pack/Unpack CLI 🔜

**文件**: `src/robocute/cli/pack.py`

**命令**:
- [ ] `rbc pack` - 打包资源到 .rbcbundle
- [ ] `rbc unpack` - 解包 .rbcbundle
- [ ] `rbc pack list` - 列出包内容

---

## Phase 7: 测试 / Testing

### Task 7.1: 单元测试 🔜

**文件**: `rbc/tests/test_resource_import.cpp`

**测试用例**:
- [ ] 导入器注册和查找
- [ ] GLTF 导入
- [ ] 资源序列化/反序列化
- [ ] 数据库操作
- [ ] 导入缓存检测
- [ ] 依赖关系追踪

### Task 7.2: 集成测试 🔜

**文件**: `rbc/tests/test_project_workflow.cpp`

**测试场景**:
- [ ] 创建新项目
- [ ] 导入资产到项目
- [ ] 加载和保存场景
- [ ] 资源打包和解包
- [ ] 项目迁移

### Task 7.3: Python 测试 🔜

**文件**: `test/test_project.py`

```python
import robocute as rbc
import tempfile
from pathlib import Path

def test_create_project():
    with tempfile.TemporaryDirectory() as tmpdir:
        project = rbc.Project.create(
            path=tmpdir,
            name="TestProject",
            author="Test User"
        )
        
        assert project.root_path().exists()
        assert (project.assets_path()).exists()
        assert (project.root_path() / "rbc_project.json").exists()

def test_import_asset():
    # TODO: Implement
    pass
```

**测试用例**:
- [ ] 项目创建
- [ ] 项目加载
- [ ] 资源导入
- [ ] CLI 工具

---

## Phase 8: 文档和示例 / Documentation and Examples

### Task 8.1: API 文档 🔜

**文件**: `docs/api/project_api.md`

**内容**:
- [ ] C++ API 参考
- [ ] Python API 参考
- [ ] CLI 命令参考
- [ ] 配置文件格式参考

### Task 8.2: 用户指南 🔜

**文件**: `docs/user-guide/project_management.md`

**内容**:
- [ ] 创建和管理项目
- [ ] 导入资产
- [ ] 资源管理
- [ ] 最佳实践

### Task 8.3: 示例项目 🔜

**目录**: `samples/example_project/`

**内容**:
- [ ] 完整的示例项目结构
- [ ] 示例资产
- [ ] 示例脚本
- [ ] README 说明

---

## 优先级和依赖关系 / Priority and Dependencies

### 高优先级 (P0) - 必须完成
1. Task 1.2: 资源导入器注册表
2. Task 2.1-2.2: 资源注册数据库
3. Task 3.1-3.2: 资源导入管理器
4. Task 4.1-4.3: 项目管理系统

### 中优先级 (P1) - 重要但可后续
1. Task 5.1-5.3: Python Bindings
2. Task 6.1-6.2: CLI 工具
3. Task 7.1-7.2: 测试

### 低优先级 (P2) - 可选或增强功能
1. Task 1.3: 更多导入器
2. Task 6.3: Pack/Unpack CLI
3. Task 8.1-8.3: 文档和示例

### 依赖关系图
```
Phase 1 (Importer Framework)
    ↓
Phase 2 (Registry Database) ←──┐
    ↓                           │
Phase 3 (Import Manager)        │
    ↓                           │
Phase 4 (Project System) ───────┘
    ↓
Phase 5 (Python Bindings)
    ↓
Phase 6 (CLI Tools)
    ↓
Phase 7 (Testing)
    ↓
Phase 8 (Documentation)
```

---

## 验收标准 / Acceptance Criteria

### Phase 1-4 完成标准
- [ ] 可以创建新项目
- [ ] 可以导入 GLTF 文件
- [ ] 资源正确序列化到 .rbcb 文件
- [ ] 资源信息正确记录到数据库
- [ ] 可以加载已导入的资源
- [ ] 变更检测正常工作（不重复导入）

### Phase 5-6 完成标准
- [ ] Python API 可用
- [ ] CLI 工具可用
- [ ] 可以通过 Python 脚本完成完整工作流
- [ ] 可以通过 CLI 完成常用操作

### Phase 7-8 完成标准
- [ ] 所有单元测试通过
- [ ] 集成测试通过
- [ ] Python 测试通过
- [ ] 文档完整
- [ ] 有可运行的示例项目

---

## 时间估算 / Time Estimation

| Phase    | 任务数 | 预估时间     |
| -------- | ------ | ------------ |
| Phase 1  | 3      | 2-3 天       |
| Phase 2  | 3      | 3-4 天       |
| Phase 3  | 3      | 4-5 天       |
| Phase 4  | 4      | 3-4 天       |
| Phase 5  | 3      | 2-3 天       |
| Phase 6  | 3      | 2-3 天       |
| Phase 7  | 3      | 3-4 天       |
| Phase 8  | 3      | 2-3 天       |
| **总计** | **25** | **21-29 天** |

---

## 开始实现 / Getting Started

1. **Fork 并 Clone 仓库**
```bash
git clone https://github.com/yourusername/RoboCute.git
cd RoboCute
```

2. **创建开发分支**
```bash
git checkout -b feature/project-system
```

3. **从 Phase 1 开始**
- 阅读 [resource_management.md](resource_management.md)
- 完成 Task 1.2: 资源导入器注册表
- 提交并测试

4. **逐步推进**
- 按照 Phase 顺序完成
- 每个 Phase 完成后进行测试
- 定期提交代码

5. **遇到问题**
- 查看相关文档
- 参考现有实现
- 在 GitHub Issues 中提问

---

**文档版本**: v1.0.0  
**最后更新**: 2025-12-31  
**作者**: RoboCute Team

