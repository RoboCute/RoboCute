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
    [[nodiscard]] bool isOpen() const { return _is_open; }

    // ========== Project Info ==========

    /**
     * Get project root directory
     */
    [[nodiscard]] const luisa::filesystem::path &projectRoot() const { return _project_root; }

    /**
     * Get project name
     */
    [[nodiscard]] const luisa::string &projectName() const { return _project_name; }

    // ========== Scene Management ==========

    /**
     * Get all scene configurations
     */
    [[nodiscard]] const luisa::vector<SceneConfig> &scenes() const { return _scenes; }

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
    [[nodiscard]] EditorScene *activeScene() { return _active_scene.has_value() ? &(*_active_scene) : nullptr; }
    [[nodiscard]] const EditorScene *activeScene() const { return _active_scene.has_value() ? &(*_active_scene) : nullptr; }

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
    bool _is_open = false;
    luisa::filesystem::path _project_root;
    luisa::string _project_name;
    luisa::vector<SceneConfig> _scenes;
    vstd::optional<EditorScene> _active_scene;

    bool loadProjectConfig();
    bool saveProjectConfig();
};

}// namespace rbc