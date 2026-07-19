#pragma once
#include <rbc_plugin/plugin.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc {
struct IProject;
struct ProjectPlugin : Plugin {
public:
    // project_root_path：项目根目录（内含 rbc_project.json）。
    // 若该目录下不存在 rbc_project.json 或加载失败，则直接报错（fail-first）。
    virtual IProject *create_project(luisa::filesystem::path const &project_root_path) = 0;
protected:
    ~ProjectPlugin() = default;
};

}// namespace rbc