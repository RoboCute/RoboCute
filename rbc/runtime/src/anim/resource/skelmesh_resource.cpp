#include "rbc_anim/resource/skelmesh_resource.h"
#include "rbc_world/type_register.h"
#include "rbc_world/resource_loader.h"

namespace rbc {

void SkelmeshResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void SkelmeshResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coro::coroutine SkelmeshResource::_async_load() {
}

bool SkelmeshResource::unsafe_save_to_path() const {
}

void SkelmeshResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkelmeshResource)

}// namespace rbc
