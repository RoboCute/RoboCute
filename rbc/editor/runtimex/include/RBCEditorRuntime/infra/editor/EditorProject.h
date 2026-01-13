#pragma once

#include <luisa/vstl/common.h>
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

struct ProjectConfig {
    luisa::string project_root;
    bool is_local_project = true;
};

struct SceneConfig {
    luisa::string scene_file;
    bool is_remote = false;
};

class EditorProject {
public:
    EditorProject(ProjectConfig config);// load from config
    ~EditorProject();

private:
    ProjectConfig config_;             // config for this project
    luisa::vector<SceneConfig> scenes_;// scenes contained in this project
    vstd::optional<EditorScene> scene; // optional opening scene
};

}// namespace rbc