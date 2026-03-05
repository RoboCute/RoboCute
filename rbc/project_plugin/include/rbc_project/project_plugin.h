#pragma once
#include <rbc_plugin/plugin.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc {
struct IProject;
struct ProjectPlugin : Plugin {
public:
    virtual IProject *create_project(luisa::filesystem::path const &assets_db_path) = 0;
protected:
    ~ProjectPlugin() = default;
};

}// namespace rbc