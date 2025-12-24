#include "rbc_anim/resource/skin_resource.h"
#include "rbc_world/type_register.h"

namespace rbc {

void SkinResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void SkinResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coro::coroutine SkinResource::_async_load() {
    co_return;
}

bool SkinResource::unsafe_save_to_path() const {
    return {};//TODO
}

void SkinResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkinResource)

}// namespace rbc
