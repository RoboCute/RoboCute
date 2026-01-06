#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
namespace rbc::world {
struct Resource;
struct RBC_RUNTIME_API Project {
private:
    luisa::filesystem::path _assets_path;
    luisa::filesystem::path _meta_path;
    luisa::vector<vstd::Guid> _resources;
    void _reimport(vstd::Guid binary_guid, luisa::filesystem::path const &origin_path);

public:
    // meta files in project
    luisa::span<vstd::Guid const> resources() const {
        return _resources;
    }
    Project(
        luisa::filesystem::path assets_path,
        luisa::filesystem::path meta_path,
        luisa::filesystem::path const &assets_db_path);
    void scan_project();
    ~Project();
    Project(Project const &) = delete;
    Project(Project &&) = delete;
};
}// namespace rbc::world