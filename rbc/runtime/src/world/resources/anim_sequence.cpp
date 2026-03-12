#include "rbc_world/resources/anim_sequence.h"
#include "rbc_world/type_register.h"
#include "luisa/core/binary_file_stream.h"
#include "rbc_core/binary_file_writer.h"

namespace rbc::world {

AnimSequenceResource::AnimSequenceResource() = default;
AnimSequenceResource::~AnimSequenceResource() {}

void AnimSequenceResource::serialize_meta(world::ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.value(ref_skel->guid(), "ref_skel");
}

void AnimSequenceResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    vstd::Guid ref_skel_guid;
    ser.ar.value(ref_skel_guid, "ref_skel");
    auto res = get_resource(ref_skel_guid, true);
    if (res && res->is_type_of(TypeInfo::get<SkeletonResource>())) {
        ref_skel = res;
    } else {
        ref_skel = nullptr;
    }
}

rbc::coroutine AnimSequenceResource::_async_load() {
    if (!ref_skel) {
        co_return;
    }
    co_await ref_skel->await_loading();

    std::shared_lock lck{_async_mtx};
    auto path = this->path();
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) { co_return; }

    luisa::BinaryBlob blob = file_stream.read(file_stream.length());
    BinDeSerializer deser{blob};
    deser._load(anim_sequence, "anim_sequence");

    co_return;
}

bool AnimSequenceResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    BinSerializer ser;
    ser._store(anim_sequence, "anim_sequence");

    auto path = this->path();
    BinaryFileWriter writer{luisa::to_string(path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    LUISA_INFO("Anim Seq Writing to {}", path.string());
    auto bytes = ser.write_to();
    writer.write(bytes);
    return true;
}

void AnimSequenceResource::log_brief() {
    anim_sequence.log_brief();
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

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimSequenceResource)

}// namespace rbc::world

bool rbc::Serialize<rbc::world::AnimSequenceResource>::write(rbc::ArchiveWrite &w, const rbc::world::AnimSequenceResource &v) {
    rbc::world::ObjSerialize ser_obj{w};
    v.serialize_meta(ser_obj);
    return true;
}
bool rbc::Serialize<rbc::world::AnimSequenceResource>::read(rbc::ArchiveRead &r, rbc::world::AnimSequenceResource &v) {
    rbc::world::ObjDeSerialize deser_obj{r};
    v.deserialize_meta(deser_obj);
    return true;
}
