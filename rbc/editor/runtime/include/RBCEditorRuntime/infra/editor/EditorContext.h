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
    ProjectCache _cache;
    LocalConfig _local_config;
    UserConfig _user_config;
    vstd::optional<EditorProject> _project;// optional opening project
};

}// namespace rbc