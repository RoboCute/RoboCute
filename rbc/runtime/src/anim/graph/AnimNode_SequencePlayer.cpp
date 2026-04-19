#include "rbc_anim/graph/AnimNode_SequencePlayer.h"
#include "rbc_anim/anim_record.h"
#include <tracy_wrapper.h>

namespace rbc {

void AnimNode_SequencePlayer::initialize_any_thread(const AnimationInitializationContext &in_context) {
    // LUISA_INFO("Initialize AnimNode_SequencePlayer");
}
void AnimNode_SequencePlayer::update_any_thread(const AnimationUpdateContext &in_context) {
    // LUISA_INFO("Updating AnimNode_SequencePlayer");
    RBCZoneScopedN("AnimNode_SequencePlayer::update_any_thread");
    // add asset player to synchronizer
    // BlendWeight = InContext.GetFinalBlendWeight()
    update_asset_player(in_context);
}

void AnimNode_SequencePlayer::evaluate_any_thread(PoseContext &output) {
    // LUISA_INFO("Evaluating AnimNode_SequencePlayer with time {}", internal_time_accumulator);

    RBCZoneScopedN("AnimNode_SequencePlayer::evaluate_any_thread");
    if (!anim_seq_resource) [[unlikely]] {
        LUISA_ERROR("Sampling on an Invalid AnimSequence!");
        output.reset_to_ref_pose();
    }
    world::AnimSequenceResource *anim_seq = anim_seq_resource.get();
    if (anim_seq != nullptr) {
        AnimationPoseData pose_data{output};
        AnimExtractContext extract_ctx;
        extract_ctx.current_time = internal_time_accumulator;
        // extract_ctx.delta_time_record = delta_time_record;

        anim_seq->ref_seq().GetAnimationPose(pose_data, extract_ctx);
    } else {
        output.reset_to_ref_pose();
    }
}

void AnimNode_SequencePlayer::node_debug() {
    // LUISA_INFO("Debugging AnimNode_SequencePlayer");
}

}// namespace rbc

// impl
namespace rbc {

void AnimNode_SequencePlayer::update_asset_player(const AnimationUpdateContext &in_context) {
    if (anim_seq_resource) {
        // temp: direct add and loop
        internal_time_accumulator += in_context.get_delta_time();
        world::AnimSequenceResource *anim = anim_seq_resource.get();
        if (internal_time_accumulator > anim->ref_seq().GetRawAnim().duration()) {
            internal_time_accumulator = 0.0f;
        }

        // TODO: use TickRecordSystem
        // create_tick_record_for_node(in_context, anim_seq_resource.get_installed(), is_looping, false);
    }
}
void AnimNode_SequencePlayer::create_tick_record_for_node(const AnimationUpdateContext &in_context, world::AnimSequenceResource *in_anim_seq_resource, bool b_looping, bool b_is_evaluator) {
    // 假设AnimSeqResource已经install
    // auto tick_record = AnimTickRecord(
    //     &(in_anim_seq_resource->animation),
    //     b_looping,
    //     1.0f,
    //     b_is_evaluator,
    //     1.0f,
    //     /**InOut */ internal_time_accumulator);
}

void AnimNode_SequencePlayer::serialize(rbc::ArchiveWrite &w) {
    w.value<world::AnimSequenceResource>(*anim_seq_resource, "ref_resource");
}

void AnimNode_SequencePlayer::deserialize(rbc::ArchiveRead &r) {
    r.value<world::AnimSequenceResource>(*anim_seq_resource, "ref_resource");
}

}// namespace rbc