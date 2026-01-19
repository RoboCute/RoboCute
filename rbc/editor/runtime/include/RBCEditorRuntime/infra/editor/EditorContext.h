#pragma once
#include "RBCEditorRuntime/infra/project/config.h"
#include "RBCEditorRuntime/infra/editor/EditorProject.h"
#include "RBCEditorRuntime/infra/editor/EditorScene.h"
#include <luisa/vstl/common.h>

// The Overall Editor Context
namespace rbc {

// previous project open log
struct ProjectCache {
};

// opening project
// user configs
struct EditorContext {
    EditorContext(luisa::string local_launch_path);
    ProjectCache cache_;
    LocalConfig local_config_;
    UserConfig user_config_;
    vstd::optional<EditorProject> project_;// optional opening project
};

}// namespace rbc