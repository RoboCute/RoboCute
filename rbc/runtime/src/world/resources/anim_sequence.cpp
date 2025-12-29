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

void AnimSequence::GetAnimationPose(AnimationPoseData &OutPoseData, const AnimExtractContext &InExtractContext) const {

    auto &pose = OutPoseData.GetPose();
    // TODO: 此处需要在Sampling中额外分配空间，需要改为事先分配
    AnimSamplingJobContext context;

    context.Resize(animation.num_tracks());
    float ratio = InExtractContext.current_time / animation.duration();
    ratio = ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f :
                                                 ratio;

    LUISA_INFO("Sampling In Animation {} with ratio {}", animation.name(), ratio);

    AnimSamplingJob sampling_job;
    sampling_job.animation = &animation;
    sampling_job.ratio = ratio;
    sampling_job.context = &context;
    luisa::vector<AnimSOATransform> &out_bones = OutPoseData.GetPose().GetBones();
    sampling_job.output = {out_bones.begin(), out_bones.end()};
    if (!sampling_job.Run()) {
        LUISA_ERROR("Failed to run SamplingJob");
    }
}

bool rbc::Serialize<rbc::AnimSequenceResource>::write(rbc::ArchiveWrite &w, const rbc::AnimSequenceResource &v) {
    w.value<rbc::AnimSequence>(v.anim_sequence, "ref_seq");
    w.value<rbc::SkeletonResource>(*v.ref_skel, "ref_skel");
    return true;
}
bool rbc::Serialize<rbc::AnimSequenceResource>::read(rbc::ArchiveRead &r, rbc::AnimSequenceResource &v) {
    r.value<rbc::AnimSequence>(v.anim_sequence, "ref_seq");
    r.value<rbc::SkeletonResource>(*v.ref_skel, "ref_skel");
    return true;
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimSequenceResource)

}// namespace rbc
