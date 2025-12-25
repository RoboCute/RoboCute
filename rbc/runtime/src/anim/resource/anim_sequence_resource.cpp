#include "rbc_anim/resource/anim_sequence_resource.h"
#include "rbc_world/type_register.h"

namespace rbc {

void AnimSequenceResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void AnimSequenceResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coroutine AnimSequenceResource::_async_load() {
    co_return;
}

bool AnimSequenceResource::unsafe_save_to_path() const {
    return {};
}

void AnimSequenceResource::_unload() {
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimSequenceResource)

}// namespace rbc
