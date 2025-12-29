#include "rbc_world/resources/anim_sequence.h"
#include "rbc_world/type_register.h"

namespace rbc {

AnimSequenceResource::AnimSequenceResource() = default;
AnimSequenceResource::~AnimSequenceResource() {}

void AnimSequenceResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void AnimSequenceResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coroutine AnimSequenceResource::_async_load() {
    co_return;
}

void AnimSequenceResource::log_brief() {
    anim_sequence.log_brief();
}

bool AnimSequenceResource::unsafe_save_to_path() const {
    return {};
}

bool rbc::Serialize<rbc::AnimSequenceResource>::write(rbc::ArchiveWrite &w, const rbc::AnimSequenceResource &v) {
    w.value<rbc::AnimSequence>(v.anim_sequence, "ref_seq");
    w.value<rbc::SkeletonResource>(*v.ref_skel, "ref_sekl");
    return true;
}
bool rbc::Serialize<rbc::AnimSequenceResource>::read(rbc::ArchiveRead &r, rbc::AnimSequenceResource &v) {
    r.value<rbc::AnimSequence>(v.anim_sequence, "ref_seq");
    r.value<rbc::SkeletonResource>(*v.ref_skel, "ref_sekl");
    return true;
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimSequenceResource)

}// namespace rbc
