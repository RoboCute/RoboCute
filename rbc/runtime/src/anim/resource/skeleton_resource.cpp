#include "rbc_anim/resource/skeleton_resource.h"
#include "rbc_world/type_register.h"

namespace rbc {

void SkeletonResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void SkeletonResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coro::coroutine SkeletonResource::_async_load() {
    co_return;
}

bool SkeletonResource::unsafe_save_to_path() const {
    return {}; // TODO
}

void SkeletonResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkeletonResource)

}// namespace rbc