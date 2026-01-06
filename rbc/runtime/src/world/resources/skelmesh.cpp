#include "rbc_world/resources/skelmesh.h"
#include "rbc_world/type_register.h"

namespace rbc {

void SkelMeshResource::serialize_meta(world::ObjSerialize const &ser) const {
}

void SkelMeshResource::deserialize_meta(world::ObjDeSerialize const &ser) {
}

rbc::coroutine SkelMeshResource::_async_load() {
    co_return;
}

bool SkelMeshResource::unsafe_save_to_path() const {
    return {};
}
// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkelMeshResource)

}// namespace rbc
