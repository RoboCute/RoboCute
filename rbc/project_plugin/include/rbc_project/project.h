#pragma once
#include <rbc_config.h>
#include <rbc_core/rc.h>
#include <luisa/core/stl/filesystem.h>
namespace rbc {
namespace world {
struct Resource;
}// namespace world
struct IProject : RCBase {
    struct FileMeta {
        vstd::Guid guid;
        vstd::string meta_info;
        vstd::Guid type_id;
    };

    IProject() = default;
    virtual luisa::filesystem::path const &root_path() const = 0;
    virtual RC<world::Resource> import_assets(
        luisa::filesystem::path origin_path,
        vstd::MD5 type_id) = 0;
    virtual void scan_project() = 0;
    virtual void unsafe_write_file_meta(
        luisa::filesystem::path dest_path,
        luisa::span<FileMeta const> metas) = 0;
    virtual ~IProject() = default;
};
}// namespace rbc