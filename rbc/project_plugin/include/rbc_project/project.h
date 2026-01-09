#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc::world {

// Configurable Project Paths
struct ProjectPaths {
    using Path = luisa::filesystem::path;
    Path root_path;
    Path asset_path;
    Path intermediate_path;
};

// Configurable Project Meta
struct ProjectMeta {
    luisa::filesystem::path project_root_path;
};

struct Project {
private:
    ProjectPaths paths_;
};

struct ProjectManager {

    luisa::vector<ProjectMeta> saved_project_metas;
};

}// namespace rbc::world