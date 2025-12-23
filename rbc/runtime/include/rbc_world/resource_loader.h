#pragma once
#include <rbc_world/resource_base.h>
namespace rbc::world {
RBC_RUNTIME_API void init_resource_loader(luisa::filesystem::path const &meta_path);
RBC_RUNTIME_API RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file = true);
RBC_RUNTIME_API void register_resource(Resource* res);

}// namespace rbc::world