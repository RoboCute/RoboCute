#pragma once

#include <luisa/vstl/common.h>
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

struct SceneConfig {
    luisa::string scene_file;
    bool is_remote = false;
};

class EditorProject {
public:
    EditorProject();// load from config
    ~EditorProject();

private:                               // config for this project
    luisa::vector<SceneConfig> scenes_;// scenes contained in this project
    vstd::optional<EditorScene> scene; // optional opening scene
};

}// namespace rbc