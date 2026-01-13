#pragma once
#include <rbc_config.h>
#include <rbc_core/rc.h>
#include <luisa/core/stl/filesystem.h>
namespace rbc::project {
struct Resource;
struct Project : RCBase {
private:
    luisa::filesystem::path _assets_path;
    void _reimport(
        vstd::Guid binary_guid,
        luisa::string &meta_data,
        vstd::MD5 type_id,
        luisa::filesystem::path const &origin_path);

public:
    Project(luisa::filesystem::path const &assets_db_path);
    void scan_project();
    ~Project();
    Project(Project const &) = delete;
    Project(Project &&) = delete;
};
}// namespace rbc::world