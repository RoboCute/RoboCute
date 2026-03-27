---
name: importer
description: Resource importer patterns for RoboCute importer_plugin module
triggers:
  - file_types: [".cpp", ".h"]
    path_patterns: ["rbc/importer_plugin/**/*"]
---

# Importer Plugin Skill for RoboCute

Guidelines for implementing resource importers in the `rbc/importer_plugin` module.

## Core Interfaces

### IResourceImporter

Base interface for all resource importers. Located in `rbc_world/resource_importer.h`.

```cpp
struct IResourceImporter {
    virtual ~IResourceImporter() = default;
    [[nodiscard]] virtual luisa::string_view extension() const = 0;
    [[nodiscard]] virtual MD5 resource_type() const = 0;
    [[nodiscard]] virtual bool can_import(luisa::filesystem::path const &path) const;
    virtual bool import(Resource *resource, luisa::filesystem::path const &path) = 0;
};
```

### Specialized Importer Interfaces

```cpp
// Mesh importers
struct IMeshImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return TypeInfo::get<MeshResource>().md5();
    }
};

// Texture importers
struct ITextureImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return TypeInfo::get<TextureResource>().md5();
    }
    virtual bool import(RC<TextureResource> resource, TextureLoader *loader,
                        luisa::filesystem::path const &path, 
                        uint mip_level, bool to_vt) = 0;
};

// Material importers
struct IMaterialImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return MD5{"rbc::world::MaterialResource"sv};
    }
};
```

## Importer Implementation Pattern

### Simple Importer Example

```cpp
struct ObjMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { 
        return ".obj"; 
    }
    
    bool import(Resource *resource_base, 
                luisa::filesystem::path const &path) override {
        auto *mesh = static_cast<MeshResource *>(resource_base);
        // Import logic here
        return true;
    }
};
```

### Multi-Format Importer (STB Image)

```cpp
struct StbTextureImporter final : ITextureImporter {
    [[nodiscard]] luisa::string_view extension() const override { 
        return ".png";  // Primary extension
    }
    
    [[nodiscard]] bool can_import(luisa::filesystem::path const &path) const override {
        // Check multiple extensions
        auto ext = path.extension().string();
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || 
               ext == ".bmp" || ext == ".gif" || ext == ".tga";
    }
    
    bool import(RC<TextureResource> resource, TextureLoader *loader,
                luisa::filesystem::path const &path,
                uint mip_level, bool to_vt) override {
        // Import logic using stb_image
        return true;
    }
};
```

## Importer Registry


### Manual Registration

```cpp
// rbc/importer_plugin/src/register_importers.cpp
void register_builtin_importers() {
    auto &registry = ResourceImporterRegistry::instance();
    
    static ObjMeshImporter obj_importer;
    registry.register_importer(&obj_importer);
    
    static StbTextureImporter texture_importer;
    registry.register_importer(&texture_importer);
}
```

## Scene Asset Importers

For complex formats (GLTF, FBX) that contain multiple resources.

### ISceneAssetImporter Interface

```cpp
struct ISceneAssetImporter {
    virtual ~ISceneAssetImporter() = default;
    
    [[nodiscard]] virtual bool can_import(
        luisa::filesystem::path const &path) const = 0;
    
    [[nodiscard]] virtual luisa::vector<luisa::string> 
        supported_extensions() const = 0;
    
    // Discover sub-assets without importing
    [[nodiscard]] virtual luisa::vector<SubAssetInfo> analyze(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const = 0;
    
    // Full import
    [[nodiscard]] virtual SceneImportResult import(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const = 0;
    
    [[nodiscard]] virtual luisa::string_view name() const = 0;
};
```

### Auto-Registration Macro

```cpp
class GltfAssetImporter : public ISceneAssetImporter {
    // Implementation...
    [[nodiscard]] luisa::string_view name() const override { 
        return "gltf"; 
    }
};

// Auto-register at startup
REGISTER_SCENE_ASSET_IMPORTER(GltfAssetImporter);
```

## GLTF Utilities

```cpp
#include <rbc_importer/gltf.h>

// Read accessor data
luisa::vector<float3> positions;
read_accessor_data<float3>(model, position_accessor_index, positions);

// Process entire model
GltfImportData import_data = process_gltf_model(model);
// import_data.mesh_builder - built mesh
// import_data.max_weight_count - skin weights
// import_data.all_joint_index, all_joint_weight - skinning data
```

## Best Practices

1. **Syntax Check**: Always use `CppSyntaxCheck` tool to check C++ file syntax after writing. Give up if file not in compile_commands.json.
2. **Use `final` for leaf classes** - Mark importer implementations as `final`
2. **Static instances for registration** - Use static instances when registering with the global registry
3. **Override `can_import`** - For multi-format importers, override to check file signatures/extensions
4. **Error handling** - Return `false` on import failure, log errors via `RBC_LOG_ERROR`
5. **Type safety** - Use `static_cast` to convert `Resource*` to specific resource type
6. **RAII registry** - Use `ImporterRegistry` for scoped registration with automatic cleanup
