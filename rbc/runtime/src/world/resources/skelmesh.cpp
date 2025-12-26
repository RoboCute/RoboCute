#include "rbc_world/resources/skelmesh.h"
#include "rbc_world/type_register.h"

namespace rbc {

void SkelMeshResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void SkelMeshResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coroutine SkelMeshResource::_async_load() {
    co_return;
}

bool SkelMeshResource::unsafe_save_to_path() const {
    return {};
}

void SkelMeshResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkelMeshResource)

}// namespace rbc
