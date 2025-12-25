#include "rbc_anim/resource/anim_graph_resource.h"
#include "rbc_world/type_register.h"

namespace rbc {

void AnimGraphResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void AnimGraphResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coroutine AnimGraphResource::_async_load() {
    co_return;
}

bool AnimGraphResource::unsafe_save_to_path() const {
    return {};//TODO
}

void AnimGraphResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimGraphResource)

}// namespace rbc
