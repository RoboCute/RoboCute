#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_core/sqlite_cpp.h>
namespace rbc::world {
struct RBC_RUNTIME_API Project {
private:
    SqliteCpp _db;
    luisa::filesystem::path _assets_path;
    luisa::filesystem::path _meta_path;
    luisa::filesystem::path _binary_path;
    bool _import_file(
        luisa::filesystem::path const &origin_file_path,
        luisa::filesystem::path const &meta_file_path,
        luisa::filesystem::path const &binary_file_path,
        vstd::Guid file_guid);

public:
    Project(
        luisa::filesystem::path assets_path,
        luisa::filesystem::path meta_path,
        luisa::filesystem::path binary_path,
        luisa::filesystem::path const &assets_db_path);
    void scan_project();
    auto const &db() const { return _db; }
    ~Project();
    Project(Project const &) = delete;
    Project(Project &&) = delete;
};
}// namespace rbc::world