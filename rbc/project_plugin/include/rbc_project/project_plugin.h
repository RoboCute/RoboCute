#pragma once
#include <rbc_plugin/plugin.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc {
struct IProject;
struct ProjectPlugin : Plugin {
public:
    // project_root_path：项目根目录（内含 rbc_project.json）。
    // 兼容：若该目录下不存在 rbc_project.json，则按 legacy 模式将其视为 assets 目录
    // （打印弃用警告；此时项目根回退为其父目录）。
    virtual IProject *create_project(luisa::filesystem::path const &project_root_path) = 0;
protected:
    ~ProjectPlugin() = default;
};

}// namespace rbc