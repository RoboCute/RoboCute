#pragma once

#include <rbc_world/resources/scene.h>
#include <rbc_importer/scene_asset_importer.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc::world {

/**
 * @brief Project-aware scene loader
 * 
 * Loads scene files (GLTF, FBX, etc.) from a project directory,
 * handling all dependencies and importing to the intermediate directory.
 */
class ProjectSceneLoader {
public:
    struct LoadConfig {
        // Import settings
        SceneImportContext::Settings import_settings;
        
        // Whether to force re-import even if cached
        bool force_reimport;
        
        // Whether to auto-save imported resources
        bool auto_save;
    };
    
    struct LoadResult {
        // The loaded scene resource
        RC<SceneResource> scene;
        
        // All imported/loaded resources
        luisa::vector<RC<Resource>> resources;
        
        // Success flag
        bool success = false;
        
        // Error message
        luisa::string error;
    };

    /**
     * @brief Construct a loader for a specific project
     * 
     * @param project_root Path to project root (contains rbc_project.json)
     */
    explicit ProjectSceneLoader(luisa::filesystem::path project_root);
    
    /**
     * @brief Load a scene from the project
     * 
     * @param relative_path Path relative to project assets directory
     *                      (e.g., "anim_test/test_anim.gltf")
     * @param config Load configuration
     * @return Load result
     */
    [[nodiscard]] LoadResult load(
        luisa::filesystem::path const &relative_path,
        LoadConfig const &config);
    
    /**
     * @brief Get the project root directory
     */
    [[nodiscard]] luisa::filesystem::path const &project_root() const { return _project_root; }
    
    /**
     * @brief Get the assets directory
     */
    [[nodiscard]] luisa::filesystem::path assets_dir() const { return _project_root / "assets"; }
    
    /**
     * @brief Get the intermediate (.rbc) directory
     */
    [[nodiscard]] luisa::filesystem::path intermediate_dir() const { return _project_root / ".rbc"; }

private:
    luisa::filesystem::path _project_root;
    
    /**
     * @brief Ensure intermediate directory structure exists
     */
    void ensure_intermediate_structure();
    
    /**
     * @brief Check if a scene needs re-import
     */
    [[nodiscard]] bool needs_reimport(
        luisa::filesystem::path const &source_path,
        luisa::filesystem::path const &imported_marker) const;
    
    /**
     * @brief Write import marker file
     */
    void write_import_marker(
        luisa::filesystem::path const &source_path,
        luisa::filesystem::path const &marker_path);
};

/**
 * @brief Convenience function to load a scene from a project
 */
[[nodiscard]] ProjectSceneLoader::LoadResult load_project_scene(
    luisa::filesystem::path const &project_root,
    luisa::filesystem::path const &scene_relative_path,
    ProjectSceneLoader::LoadConfig const &config);

} // namespace rbc::world
