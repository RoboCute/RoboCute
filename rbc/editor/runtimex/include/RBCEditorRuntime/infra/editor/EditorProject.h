#pragma once

#include <rbc_config.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/filesystem.h>
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

/**
 * Scene configuration entry in a project
 */
struct SceneConfig {
    luisa::string scene_name;                      // Display name
    luisa::string scene_file;                      // File path (relative to project root)
    EditorSceneSourceType source_type = EditorSceneSourceType::Local;
    luisa::string server_url;                      // For remote scenes: server URL
    bool is_default = false;                       // Whether this is the default scene to open
};

/**
 * EditorProject - Represents an open project in the editor
 * 
 * A project contains:
 * - One or more scenes (local or remote)
 * - Project-level configuration
 * - Resource paths and settings
 * 
 * The project manages scene lifecycle and provides access to the active scene
 * through the SceneService.
 */
class RBC_EDITOR_RUNTIME_API EditorProject {
public:
    EditorProject();
    ~EditorProject();

    // ========== Project Lifecycle ==========

    /**
     * Load project from a project file or directory
     * @param projectPath Path to project folder or .rbcproj file
     * @return true if project loaded successfully
     */
    bool load(const luisa::filesystem::path &projectPath);

    /**
     * Close the current project
     */
    void close();

    /**
     * Check if a project is currently open
     */
    [[nodiscard]] bool isOpen() const { return is_open_; }

    // ========== Project Info ==========

    /**
     * Get project root directory
     */
    [[nodiscard]] const luisa::filesystem::path &projectRoot() const { return project_root_; }

    /**
     * Get project name
     */
    [[nodiscard]] const luisa::string &projectName() const { return project_name_; }

    // ========== Scene Management ==========

    /**
     * Get all scene configurations
     */
    [[nodiscard]] const luisa::vector<SceneConfig> &scenes() const { return scenes_; }

    /**
     * Get the default scene config (if any)
     */
    [[nodiscard]] const SceneConfig *defaultScene() const;

    /**
     * Find scene config by name
     */
    [[nodiscard]] const SceneConfig *findScene(const luisa::string &name) const;

    /**
     * Add a new scene configuration
     */
    void addScene(const SceneConfig &config);

    /**
     * Remove a scene configuration by name
     */
    bool removeScene(const luisa::string &name);

    // ========== Active Scene ==========

    /**
     * Get the currently active scene (if any)
     */
    [[nodiscard]] EditorScene *activeScene() { return active_scene_.has_value() ? &(*active_scene_) : nullptr; }
    [[nodiscard]] const EditorScene *activeScene() const { return active_scene_.has_value() ? &(*active_scene_) : nullptr; }

    /**
     * Open a scene by name
     * @return true if scene opened successfully
     */
    bool openScene(const luisa::string &name);

    /**
     * Close the active scene
     */
    void closeActiveScene();

private:
    bool is_open_ = false;
    luisa::filesystem::path project_root_;
    luisa::string project_name_;
    luisa::vector<SceneConfig> scenes_;
    vstd::optional<EditorScene> active_scene_;

    bool loadProjectConfig();
    bool saveProjectConfig();
};

}// namespace rbc