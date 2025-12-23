#pragma once
#include <rbc_world_v2/resource_base.h>
namespace rbc::world {
RBC_WORLD_API void init_resource_loader(luisa::filesystem::path const &root_path, luisa::filesystem::path const &meta_path);
RBC_WORLD_API RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file = true);

}// namespace rbc::world