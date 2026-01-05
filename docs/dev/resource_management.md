# Resource Management System - Implementation Guide

## 概述 / Overview

本文档详细说明 RoboCute 的资源管理系统实现，包括资源导入、加载、序列化、数据库管理等核心组件。

This document details the implementation of RoboCute's resource management system, including resource import, loading, serialization, and database management.

---

## 1. 资源生命周期 / Resource Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│                    Resource Lifecycle                        │
└─────────────────────────────────────────────────────────────┘

[Asset File] (.gltf/.fbx/etc.)
     ↓
     ├──→ [Import Detection]
     │    - Check .meta file for GUID
     │    - Check import_cache for changes
     │    - Generate GUID if new
     │
     ├──→ [Resource Import]
     │    - Find appropriate importer
     │    - Create Resource objects
     │    - Process asset data
     │    - Generate runtime data
     │
     ├──→ [Resource Registration]
     │    - Register in world system (InstanceID + GUID)
     │    - Add to resource_registry.db
     │    - Record dependencies
     │
     ├──→ [Resource Serialization]
     │    - Serialize to .rbcb file
     │    - Store in .rbc/resources/{type}/{guid}.rbcb
     │    - Update file_offset if in bundle
     │
     └──→ [Resource Loading]
          - Query registry for path
          - Deserialize from .rbcb
          - Initialize device resources
          - Set status to Loaded

[Runtime Usage]
     ↓
[Resource Unload]
     - Release device resources
     - Keep metadata in memory

[Resource Cleanup]
     - Remove from world system
     - Cleanup memory
```

---

## 2. 核心类和接口 / Core Classes and Interfaces

### 2.1 Resource Base Class

```cpp
// rbc/runtime/include/rbc_world/resource_base.h

namespace rbc::world {

enum struct EResourceLoadingStatus : uint8_t {
    Unloaded,  // 未加载
    Loading,   // 加载中
    Loaded     // 已加载
};

struct Resource : BaseObject {
protected:
    std::atomic<EResourceLoadingStatus> _status{Unloaded};
    luisa::filesystem::path _path;      // 资源文件路径
    uint64_t _file_offset{};            // 文件内偏移（用于bundle）
    
    virtual rbc::coroutine _async_load() = 0;
    virtual bool unsafe_save_to_path() const = 0;

public:
    // 获取加载状态
    EResourceLoadingStatus loading_status() const;
    bool loaded() const;
    
    // 异步等待加载完成
    ResourceAwait await_loading();
    
    // 序列化和反序列化
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    
    // 保存到文件
    bool save_to_path();

    // 使用注册的导入器解码资源
    bool decode(luisa::filesystem::path const &path);
    
    // 路径和偏移访问
    const luisa::filesystem::path& path() const { return _path; }
    uint64_t file_offset() const { return _file_offset; }
};

// 资源加载和注册
RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file = true);
void register_resource(Resource *res);

} // namespace rbc::world
```

### 2.2 Resource Importer Interface

```cpp
// rbc/runtime/include/rbc_world/resource_importer.h

namespace rbc::world {

enum class ResourceType : uint32_t {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Material = 3,
    Shader = 4,
    AnimSequence = 5,
    Skeleton = 6,
    Skin = 9,
    AnimGraph = 10,
    SkelMesh = 11,
    Custom = 1000
};

// 基础导入器接口
struct IResourceImporter {
    virtual ~IResourceImporter() = default;
    virtual luisa::string_view extension() const = 0;
    virtual ResourceType resource_type() const = 0;
    virtual bool can_import(luisa::filesystem::path const &path) const;
};

// 网格导入器接口
struct IMeshImporter : IResourceImporter {
    ResourceType resource_type() const override { return ResourceType::Mesh; }
    virtual bool import(MeshResource *resource, luisa::filesystem::path const &path) = 0;
};

// 纹理导入器接口
struct ITextureImporter : IResourceImporter {
    ResourceType resource_type() const override { return ResourceType::Texture; }
    virtual bool import(
        RC<TextureResource> resource,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) = 0;
};

// 导入器注册表
struct ResourceImporterRegistry {
    using ExtensionKey = luisa::string;
    using ImporterKey = std::pair<ExtensionKey, ResourceType>;
    
    void register_importer(IResourceImporter *importer);
    void unregister_importer(luisa::string_view extension, ResourceType type);
    
    IResourceImporter* find_importer(
        luisa::string_view extension, 
        ResourceType type) const;
    
    IResourceImporter* find_importer(
        luisa::filesystem::path const &path, 
        ResourceType type) const;
    
    static ResourceImporterRegistry& instance();
};

} // namespace rbc::world
```

---

## 3. 资源导入实现 / Resource Import Implementation

### 3.1 注册导入器 / Register Importers

```cpp
// rbc/runtime/src/world/importers/register_importers.cpp

namespace rbc::world {

void register_builtin_importers() {
    auto& registry = ResourceImporterRegistry::instance();
    
    // Mesh importers
    static GltfMeshImporter gltf_mesh_importer;
    static GlbMeshImporter glb_mesh_importer;
    static ObjMeshImporter obj_mesh_importer;
    
    registry.register_importer(&gltf_mesh_importer);
    registry.register_importer(&glb_mesh_importer);
    registry.register_importer(&obj_mesh_importer);
    
    // Skeleton importers
    static GltfSkeletonImporter gltf_skeleton_importer;
    registry.register_importer(&gltf_skeleton_importer);
    
    // Skin importers
    static GltfSkinImporter gltf_skin_importer;
    registry.register_importer(&gltf_skin_importer);
    
    // Animation importers
    static GltfAnimSequenceImporter gltf_anim_importer;
    registry.register_importer(&gltf_anim_importer);
    
    // Texture importers
    static ImageTextureImporter image_importer;  // .png, .jpg
    static ExrTextureImporter exr_importer;      // .exr
    static TiffTextureImporter tiff_importer;    // .tiff
    
    registry.register_importer(&image_importer);
    registry.register_importer(&exr_importer);
    registry.register_importer(&tiff_importer);
}

} // namespace rbc::world
```

### 3.2 实现自定义导入器 / Implement Custom Importer

```cpp
// Example: GLTF Mesh Importer

namespace rbc::world {

struct GltfMeshImporter : IMeshImporter {
    luisa::string_view extension() const override {
        return ".gltf";
    }
    
    bool import(MeshResource *resource, luisa::filesystem::path const &path) override {
        if (!resource || !resource->empty()) {
            LUISA_WARNING("Cannot import into non-empty mesh resource");
            return false;
        }
        
        // 1. Load GLTF model
        tinygltf::Model model;
        if (!load_gltf_model(model, path, false)) {
            return false;
        }
        
        // 2. Process GLTF data
        GltfImportData import_data = process_gltf_model(model);
        
        // 3. Build mesh resource
        MeshBuilder& mesh_builder = import_data.mesh_builder;
        if (mesh_builder.position.empty()) {
            return false;
        }
        
        // 4. Write to resource
        luisa::vector<uint> submesh_offsets;
        luisa::vector<std::byte> resource_bytes;
        mesh_builder.write_to(resource_bytes, submesh_offsets);
        
        resource->create_empty(
            {}, 
            std::move(submesh_offsets), 
            mesh_builder.vertex_count(), 
            mesh_builder.indices_count() / 3,
            mesh_builder.uv_count(),
            mesh_builder.contained_normal(),
            mesh_builder.contained_tangent()
        );
        
        *(resource->host_data()) = std::move(resource_bytes);
        
        // 5. Handle additional data (skinning, etc.)
        if (import_data.max_weight_count > 0) {
            // Add joint index property
            auto property = resource->add_property(
                "joint_index",
                import_data.max_weight_count * resource->vertex_count() * sizeof(uint16_t)
            );
            std::memcpy(
                property.second.data(), 
                import_data.all_joint_index.data(), 
                property.second.size()
            );
            
            // Add joint weight property
            auto w_property = resource->add_property(
                "joint_weight",
                import_data.max_weight_count * resource->vertex_count() * sizeof(float)
            );
            std::memcpy(
                w_property.second.data(),
                import_data.all_joint_weight.data(),
                w_property.second.size()
            );
        }
        
        return true;
    }
};

} // namespace rbc::world
```

---

## 4. 资源注册数据库 / Resource Registry Database

### 4.1 数据库管理器 / Database Manager

```cpp
// rbc/runtime/include/rbc_world/resource_registry_db.h

namespace rbc::world {

struct ResourceRegistryDB {
public:
    ResourceRegistryDB(luisa::filesystem::path const& db_path);
    ~ResourceRegistryDB();
    
    // Resource operations
    bool add_resource(ResourceInfo const& info);
    bool update_resource(vstd::Guid const& guid, ResourceInfo const& info);
    bool remove_resource(vstd::Guid const& guid);
    std::optional<ResourceInfo> get_resource(vstd::Guid const& guid);
    
    // Dependency operations
    bool add_dependency(vstd::Guid const& resource, vstd::Guid const& depends_on);
    bool remove_dependency(vstd::Guid const& resource, vstd::Guid const& depends_on);
    luisa::vector<vstd::Guid> get_dependencies(vstd::Guid const& resource);
    luisa::vector<vstd::Guid> get_dependents(vstd::Guid const& resource);
    
    // Asset to resource mapping
    bool add_asset_mapping(luisa::string const& asset_path, vstd::Guid const& guid);
    luisa::vector<vstd::Guid> get_resources_from_asset(luisa::string const& asset_path);
    
    // Import cache
    bool update_import_cache(luisa::string const& asset_path, ImportCacheEntry const& entry);
    std::optional<ImportCacheEntry> get_import_cache(luisa::string const& asset_path);
    bool needs_reimport(luisa::string const& asset_path);
    
    // Queries
    luisa::vector<ResourceInfo> get_all_resources();
    luisa::vector<ResourceInfo> get_resources_by_type(luisa::string const& type);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

struct ResourceInfo {
    vstd::Guid guid;
    luisa::string type;
    luisa::string source_asset_path;
    luisa::string resource_file_path;
    uint64_t file_offset;
    uint64_t size;
    luisa::string hash;
    uint64_t created_at;
    uint64_t modified_at;
    luisa::string import_settings;  // JSON
    luisa::string metadata;         // JSON
};

struct ImportCacheEntry {
    luisa::string asset_path;
    luisa::string file_hash;
    uint64_t file_size;
    uint64_t modified_time;
    uint64_t last_import_time;
};

} // namespace rbc::world
```

### 4.2 数据库实现 / Database Implementation

```cpp
// rbc/runtime/src/world/resource_registry_db.cpp

namespace rbc::world {

struct ResourceRegistryDB::Impl {
    sqlite3* db = nullptr;
    
    Impl(luisa::filesystem::path const& db_path) {
        int rc = sqlite3_open(db_path.string().c_str(), &db);
        if (rc != SQLITE_OK) {
            LUISA_ERROR("Failed to open resource registry database: {}", 
                        sqlite3_errmsg(db));
            return;
        }
        
        initialize_schema();
    }
    
    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }
    
    void initialize_schema() {
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
    
    bool execute(const char* sql, 
                 std::function<bool(sqlite3_stmt*)> bind_params = nullptr,
                 std::function<void(sqlite3_stmt*)> process_row = nullptr) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            LUISA_WARNING("Failed to prepare SQL: {}", sqlite3_errmsg(db));
            return false;
        }
        
        if (bind_params && !bind_params(stmt)) {
            sqlite3_finalize(stmt);
            return false;
        }
        
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            if (process_row) {
                process_row(stmt);
            }
        }
        
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }
};

bool ResourceRegistryDB::add_resource(ResourceInfo const& info) {
    const char* sql = R"(
        INSERT INTO resources (guid, type, source_asset_path, resource_file_path, 
                              file_offset, size, hash, import_settings, metadata)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    return _impl->execute(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, info.guid.to_string().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, info.type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, info.source_asset_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, info.resource_file_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, info.file_offset);
        sqlite3_bind_int64(stmt, 6, info.size);
        sqlite3_bind_text(stmt, 7, info.hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, info.import_settings.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, info.metadata.c_str(), -1, SQLITE_TRANSIENT);
        return true;
    });
}

bool ResourceRegistryDB::needs_reimport(luisa::string const& asset_path) {
    auto cached = get_import_cache(asset_path);
    if (!cached) {
        return true;  // No cache, needs import
    }
    
    // Check file metadata
    std::error_code ec;
    auto file_time = luisa::filesystem::last_write_time(asset_path, ec);
    if (ec) {
        return true;
    }
    
    auto file_size = luisa::filesystem::file_size(asset_path, ec);
    if (ec) {
        return true;
    }
    
    uint64_t mtime = std::chrono::duration_cast<std::chrono::seconds>(
        file_time.time_since_epoch()).count();
    
    // Compare with cache
    if (mtime != cached->modified_time || file_size != cached->file_size) {
        return true;
    }
    
    // TODO: Optionally compute and compare file hash
    
    return false;
}

} // namespace rbc::world
```

---

## 5. 资源导入管理器 / Resource Import Manager

### 5.1 导入管理器接口 / Import Manager Interface

```cpp
// rbc/runtime/include/rbc_world/resource_import_manager.h

namespace rbc::world {

struct ImportOptions {
    bool force_reimport = false;
    bool async_import = true;
    bool generate_thumbnails = true;
    bool update_dependencies = true;
    luisa::string import_settings_json;  // Custom import settings
};

struct ImportResult {
    bool success = false;
    luisa::vector<vstd::Guid> imported_resources;
    luisa::string error_message;
    double import_time_seconds = 0.0;
};

struct ResourceImportManager {
public:
    ResourceImportManager(
        luisa::filesystem::path const& project_root,
        luisa::filesystem::path const& intermediate_dir);
    
    ~ResourceImportManager();
    
    // Import single asset
    ImportResult import_asset(
        luisa::filesystem::path const& asset_path,
        ImportOptions const& options = {});
    
    // Import multiple assets
    ImportResult import_assets(
        luisa::span<const luisa::filesystem::path> asset_paths,
        ImportOptions const& options = {});
    
    // Import all assets in directory
    ImportResult import_directory(
        luisa::filesystem::path const& directory,
        bool recursive = true,
        ImportOptions const& options = {});
    
    // Reimport all assets
    ImportResult reimport_all(ImportOptions const& options = {});
    
    // Get resources from asset
    luisa::vector<vstd::Guid> get_resources_from_asset(
        luisa::filesystem::path const& asset_path);
    
    // Check if asset needs reimport
    bool needs_reimport(luisa::filesystem::path const& asset_path);
    
    // Get import statistics
    struct ImportStats {
        uint64_t total_assets;
        uint64_t total_resources;
        uint64_t total_size_bytes;
        double total_import_time_seconds;
    };
    ImportStats get_import_stats();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace rbc::world
```

### 5.2 导入管理器实现 / Import Manager Implementation

```cpp
// rbc/runtime/src/world/resource_import_manager.cpp

namespace rbc::world {

struct ResourceImportManager::Impl {
    luisa::filesystem::path project_root;
    luisa::filesystem::path intermediate_dir;
    luisa::filesystem::path resources_dir;
    std::unique_ptr<ResourceRegistryDB> registry_db;
    
    Impl(luisa::filesystem::path const& root, luisa::filesystem::path const& inter_dir)
        : project_root(root),
          intermediate_dir(inter_dir),
          resources_dir(inter_dir / "resources") {
        
        // Create directories
        std::error_code ec;
        luisa::filesystem::create_directories(resources_dir, ec);
        
        // Initialize database
        auto db_path = resources_dir / "resource_registry.db";
        registry_db = std::make_unique<ResourceRegistryDB>(db_path);
    }
    
    ImportResult import_single_asset(
        luisa::filesystem::path const& asset_path,
        ImportOptions const& options) {
        
        ImportResult result;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 1. Check if needs reimport
        if (!options.force_reimport && !registry_db->needs_reimport(asset_path.string())) {
            LUISA_INFO("Asset {} is up to date, skipping import", asset_path.string());
            result.success = true;
            result.imported_resources = registry_db->get_resources_from_asset(asset_path.string());
            return result;
        }
        
        // 2. Load or generate asset metadata
        auto meta_path = luisa::filesystem::path(asset_path.string() + ".meta");
        AssetMetadata metadata;
        if (luisa::filesystem::exists(meta_path)) {
            metadata = load_asset_metadata(meta_path);
        } else {
            metadata.guid = vstd::Guid::create();
            metadata.import_settings = options.import_settings_json;
            save_asset_metadata(meta_path, metadata);
        }
        
        // 3. Detect asset type and find importers
        auto extension = asset_path.extension().string();
        auto& importer_registry = ResourceImporterRegistry::instance();
        
        // Try different resource types
        luisa::vector<ResourceType> types_to_try = {
            ResourceType::Mesh,
            ResourceType::Skeleton,
            ResourceType::Skin,
            ResourceType::AnimSequence,
            ResourceType::Texture
        };
        
        for (auto type : types_to_try) {
            auto importer = importer_registry.find_importer(extension, type);
            if (!importer || !importer->can_import(asset_path)) {
                continue;
            }
            
            // 4. Create resource and import
            auto resource = create_resource_for_type(type, metadata.guid);
            if (!resource) {
                continue;
            }
            
            bool import_success = false;
            switch (type) {
                case ResourceType::Mesh: {
                    auto mesh_importer = static_cast<IMeshImporter*>(importer);
                    auto mesh = static_cast<MeshResource*>(resource.get());
                    import_success = mesh_importer->import(mesh, asset_path);
                    break;
                }
                case ResourceType::Texture: {
                    // Handle texture import
                    break;
                }
                // ... other types
            }
            
            if (!import_success) {
                LUISA_WARNING("Failed to import {} as {}", 
                             asset_path.string(), 
                             resource_type_to_string(type));
                continue;
            }
            
            // 5. Serialize resource to .rbcb
            auto resource_filename = fmt::format("{}.rbcb", resource->guid().to_string());
            auto resource_path = resources_dir / resource_type_to_string(type) / resource_filename;
            
            luisa::filesystem::create_directories(resource_path.parent_path());
            
            if (!resource->save_to_path()) {
                result.error_message = fmt::format("Failed to save resource to {}", 
                                                   resource_path.string());
                continue;
            }
            
            // 6. Register in database
            ResourceInfo info;
            info.guid = resource->guid();
            info.type = resource_type_to_string(type);
            info.source_asset_path = asset_path.string();
            info.resource_file_path = resource_path.string();
            info.file_offset = 0;
            info.size = luisa::filesystem::file_size(resource_path);
            info.import_settings = options.import_settings_json;
            
            registry_db->add_resource(info);
            registry_db->add_asset_mapping(asset_path.string(), resource->guid());
            
            result.imported_resources.push_back(resource->guid());
        }
        
        // 7. Update import cache
        ImportCacheEntry cache_entry;
        cache_entry.asset_path = asset_path.string();
        cache_entry.file_size = luisa::filesystem::file_size(asset_path);
        cache_entry.modified_time = std::chrono::duration_cast<std::chrono::seconds>(
            luisa::filesystem::last_write_time(asset_path).time_since_epoch()).count();
        cache_entry.last_import_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        registry_db->update_import_cache(asset_path.string(), cache_entry);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.import_time_seconds = std::chrono::duration<double>(end_time - start_time).count();
        result.success = !result.imported_resources.empty();
        
        LUISA_INFO("Imported {} resources from {} in {:.2f}s", 
                   result.imported_resources.size(),
                   asset_path.string(),
                   result.import_time_seconds);
        
        return result;
    }
};

ImportResult ResourceImportManager::import_asset(
    luisa::filesystem::path const& asset_path,
    ImportOptions const& options) {
    return _impl->import_single_asset(asset_path, options);
}

ImportResult ResourceImportManager::import_directory(
    luisa::filesystem::path const& directory,
    bool recursive,
    ImportOptions const& options) {
    
    ImportResult result;
    luisa::vector<luisa::filesystem::path> assets_to_import;
    
    // Collect all assets
    for (auto const& entry : luisa::filesystem::directory_iterator(directory)) {
        if (entry.is_directory() && recursive) {
            auto sub_result = import_directory(entry.path(), recursive, options);
            result.imported_resources.insert(
                result.imported_resources.end(),
                sub_result.imported_resources.begin(),
                sub_result.imported_resources.end());
        } else if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            // Skip .meta files
            if (ext != ".meta") {
                assets_to_import.push_back(entry.path());
            }
        }
    }
    
    // Import assets
    for (auto const& asset_path : assets_to_import) {
        auto import_result = import_asset(asset_path, options);
        result.imported_resources.insert(
            result.imported_resources.end(),
            import_result.imported_resources.begin(),
            import_result.imported_resources.end());
    }
    
    result.success = true;
    return result;
}

} // namespace rbc::world
```

---

## 6. Python API 绑定 / Python API Bindings

```python
# src/robocute/resource_management.py

from typing import List, Optional, Dict
import robocute_ext as _rbc_ext

class ResourceImportManager:
    """Resource import manager for RoboCute projects"""
    
    def __init__(self, project_root: str, intermediate_dir: str = ".rbc"):
        """Initialize resource import manager
        
        Args:
            project_root: Path to project root directory
            intermediate_dir: Path to intermediate directory (relative to project_root)
        """
        self._impl = _rbc_ext.ResourceImportManager(project_root, intermediate_dir)
    
    def import_asset(self, 
                    asset_path: str, 
                    force_reimport: bool = False,
                    import_settings: Optional[Dict] = None) -> Dict:
        """Import a single asset
        
        Args:
            asset_path: Path to the asset file
            force_reimport: Force reimport even if up to date
            import_settings: Custom import settings
            
        Returns:
            Dictionary with keys: 'success', 'resources', 'error', 'time'
        """
        options = {
            'force_reimport': force_reimport,
            'import_settings': import_settings or {}
        }
        return self._impl.import_asset(asset_path, options)
    
    def import_directory(self,
                        directory: str,
                        recursive: bool = True,
                        force_reimport: bool = False) -> Dict:
        """Import all assets in a directory
        
        Args:
            directory: Path to the directory
            recursive: Recursively import subdirectories
            force_reimport: Force reimport even if up to date
            
        Returns:
            Dictionary with import results
        """
        options = {'force_reimport': force_reimport}
        return self._impl.import_directory(directory, recursive, options)
    
    def get_resources_from_asset(self, asset_path: str) -> List[str]:
        """Get all resources generated from an asset
        
        Args:
            asset_path: Path to the asset file
            
        Returns:
            List of resource GUIDs
        """
        return self._impl.get_resources_from_asset(asset_path)
    
    def needs_reimport(self, asset_path: str) -> bool:
        """Check if an asset needs to be reimported
        
        Args:
            asset_path: Path to the asset file
            
        Returns:
            True if reimport is needed
        """
        return self._impl.needs_reimport(asset_path)

# Usage example
if __name__ == "__main__":
    import robocute as rbc
    
    # Initialize project
    project_root = "D:/Projects/MyRobotProject"
    importer = ResourceImportManager(project_root)
    
    # Import single asset
    result = importer.import_asset("assets/models/robot.gltf")
    print(f"Imported {len(result['resources'])} resources in {result['time']:.2f}s")
    
    # Import all assets
    result = importer.import_directory("assets", recursive=True)
    print(f"Total imported: {len(result['resources'])} resources")
    
    # Get resources from asset
    resources = importer.get_resources_from_asset("assets/models/robot.gltf")
    for guid in resources:
        resource = rbc.load_resource(guid)
        print(f"Resource: {guid}, Type: {resource.type_name()}")
```

---

## 7. 命令行工具 / Command Line Tools

```python
# src/robocute/cli/import_assets.py

import click
import robocute as rbc
from robocute.resource_management import ResourceImportManager

@click.group()
def cli():
    """RoboCute asset import tool"""
    pass

@cli.command()
@click.option('--project', required=True, help='Project root directory')
@click.option('--asset', required=True, help='Path to asset file')
@click.option('--force', is_flag=True, help='Force reimport')
def import_asset(project, asset, force):
    """Import a single asset"""
    importer = ResourceImportManager(project)
    result = importer.import_asset(asset, force_reimport=force)
    
    if result['success']:
        click.echo(f"✓ Imported {len(result['resources'])} resources")
        for guid in result['resources']:
            click.echo(f"  - {guid}")
    else:
        click.echo(f"✗ Import failed: {result.get('error', 'Unknown error')}", err=True)

@cli.command()
@click.option('--project', required=True, help='Project root directory')
@click.option('--directory', required=True, help='Directory to import')
@click.option('--recursive/--no-recursive', default=True, help='Recursive import')
@click.option('--force', is_flag=True, help='Force reimport')
def import_dir(project, directory, recursive, force):
    """Import all assets in a directory"""
    importer = ResourceImportManager(project)
    result = importer.import_directory(directory, recursive=recursive, force_reimport=force)
    
    if result['success']:
        click.echo(f"✓ Imported {len(result['resources'])} resources")
    else:
        click.echo(f"✗ Import failed", err=True)

@cli.command()
@click.option('--project', required=True, help='Project root directory')
def list_resources(project):
    """List all imported resources"""
    # TODO: Implement listing
    pass

if __name__ == '__main__':
    cli()
```

使用示例：

```bash
# Import single asset
python -m robocute.cli.import_assets import-asset \
    --project D:/Projects/MyRobotProject \
    --asset assets/models/robot.gltf

# Import directory
python -m robocute.cli.import_assets import-dir \
    --project D:/Projects/MyRobotProject \
    --directory assets/ \
    --recursive

# Force reimport all
python -m robocute.cli.import_assets import-dir \
    --project D:/Projects/MyRobotProject \
    --directory assets/ \
    --force
```

---

## 8. 最佳实践 / Best Practices

### 8.1 导入性能优化 / Import Performance Optimization

1. **批量导入**: 使用 `import_directory` 而不是多次调用 `import_asset`
2. **异步导入**: 启用 `async_import` 选项
3. **缓存验证**: 利用 import_cache 避免不必要的重新导入
4. **并行处理**: 对于独立的资产，可以并行导入

### 8.2 资源管理 / Resource Management

1. **GUID 稳定性**: 保持 .meta 文件，确保资源 GUID 不变
2. **依赖追踪**: 正确记录资源依赖关系
3. **定期清理**: 删除未使用的资源和缓存
4. **备份数据库**: 定期备份 resource_registry.db

### 8.3 错误处理 / Error Handling

1. **验证输入**: 检查文件路径、格式、权限
2. **记录日志**: 详细记录导入过程和错误
3. **回滚机制**: 导入失败时清理部分导入的资源
4. **用户反馈**: 提供清晰的错误信息和建议

---

**文档版本**: v1.0.0  
**最后更新**: 2025-12-31  
**作者**: RoboCute Team

