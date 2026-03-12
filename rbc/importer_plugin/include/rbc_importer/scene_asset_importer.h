#pragma once

#include <rbc_world/resource_base.h>
#include <rbc_world/resource_importer.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>

namespace rbc::world {

// Forward declarations
struct SceneImportContext;
struct SceneImportResult;

/**
 * @brief Represents a discovered sub-asset within a scene file
 * 
 * Scene files (GLTF, FBX, etc.) often contain or reference multiple resources.
 * This structure describes one such resource.
 */
struct SubAssetInfo {
    // Resource type
    enum class Type {
        Unknown,
        Mesh,
        Material,
        Texture,
        Skeleton,
        Skin,
        Animation,
        Scene
    } type = Type::Unknown;
    
    // Unique name within the scene (e.g., "Cylinder", "Material", "Armature")
    luisa::string name;
    
    // Original file path (for external references like textures)
    luisa::filesystem::path source_path;
    
    // Index in the original file (for embedded resources)
    int32_t index = -1;
    
    // Parent scene file path
    luisa::filesystem::path parent_scene;
    
    // Generated GUID after import
    vstd::Guid imported_guid;
};

/**
 * @brief Context for scene import operations
 * 
 * Provides access to the project structure and import settings.
 */
struct SceneImportContext {
    // Project root directory (contains rbc_project.json)
    luisa::filesystem::path project_root;
    
    // Intermediate directory for imported resources (.rbc/)
    luisa::filesystem::path intermediate_dir;
    
    // Source asset directory (e.g., assets/anim_test/)
    luisa::filesystem::path source_dir;
    
    // Import settings
    struct Settings {
        bool import_meshes = true;
        bool import_materials = true;
        bool import_textures = true;
        bool import_skeletons = true;
        bool import_skins = true;
        bool import_animations = true;
        
        // Texture settings
        uint32_t texture_mip_levels = 4;
        bool texture_compress = true;
        
        // Mesh settings
        bool mesh_optimize = true;
    } settings;
};

/**
 * @brief Result of a scene import operation
 */
struct SceneImportResult {
    // The main scene resource (if created)
    RC<Resource> main_resource;
    
    // All imported sub-resources
    luisa::vector<RC<Resource>> resources;
    
    // Mapping from original names to imported GUIDs
    luisa::unordered_map<luisa::string, vstd::Guid> name_to_guid;
    
    // Import success flag
    bool success = false;
    
    // Error message if failed
    luisa::string error_message;
};

/**
 * @brief Base interface for scene asset importers (GLTF, FBX, etc.)
 * 
 * Scene asset importers handle complex file formats that may contain
 * or reference multiple resources (meshes, materials, textures, etc.).
 */
struct ISceneAssetImporter {
    virtual ~ISceneAssetImporter() = default;
    
    /**
     * @brief Check if this importer can handle the given file
     */
    [[nodiscard]] virtual bool can_import(luisa::filesystem::path const &path) const = 0;
    
    /**
     * @brief Get the supported file extensions
     */
    [[nodiscard]] virtual luisa::vector<luisa::string> supported_extensions() const = 0;
    
    /**
     * @brief Analyze the scene file and discover all sub-assets
     * 
     * This does not import the resources, just discovers what would be imported.
     * 
     * @param path Path to the scene file
     * @param ctx Import context
     * @return List of discovered sub-assets
     */
    [[nodiscard]] virtual luisa::vector<SubAssetInfo> analyze(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const = 0;
    
    /**
     * @brief Import a scene file and all its dependencies
     * 
     * @param path Path to the scene file
     * @param ctx Import context
     * @return Import result containing all imported resources
     */
    [[nodiscard]] virtual SceneImportResult import(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const = 0;
    
    /**
     * @brief Get the importer name
     */
    [[nodiscard]] virtual luisa::string_view name() const = 0;
};

/**
 * @brief Registry for scene asset importers
 */
struct SceneAssetImporterRegistry {
    using ImporterPtr = luisa::unique_ptr<ISceneAssetImporter>;
    
    static SceneAssetImporterRegistry &instance();
    
    /**
     * @brief Register a scene importer
     */
    void register_importer(ImporterPtr importer);
    
    /**
     * @brief Find an importer for the given file
     */
    [[nodiscard]] ISceneAssetImporter *find_importer(luisa::filesystem::path const &path) const;
    
    /**
     * @brief Find an importer by name
     */
    [[nodiscard]] ISceneAssetImporter *find_importer_by_name(luisa::string_view name) const;
    
    /**
     * @brief Get all registered importers
     */
    [[nodiscard]] luisa::span<const ImporterPtr> all_importers() const;
    
private:
    SceneAssetImporterRegistry() = default;
    luisa::vector<ImporterPtr> _importers;
};

/**
 * @brief Helper to register scene importers automatically
 */
#define REGISTER_SCENE_ASSET_IMPORTER(importer_class) \
    static struct _##importer_class##_registrar { \
        _##importer_class##_registrar() { \
            SceneAssetImporterRegistry::instance().register_importer( \
                luisa::make_unique<importer_class>()); \
        } \
    } _##importer_class##_instance;

} // namespace rbc::world
