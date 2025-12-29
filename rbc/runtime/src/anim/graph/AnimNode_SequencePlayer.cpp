#include "rbc_anim/graph/AnimNode_SequencePlayer.h"
#include <tracy_wrapper.h>

namespace rbc {

void AnimNode_SequencePlayer::Initialize_AnyThread(const AnimationInitializationContext &InContext) {
    // LUISA_INFO("Initialize AnimNode_SequencePlayer");
}
void AnimNode_SequencePlayer::Update_AnyThread(const AnimationUpdateContext &InContext) {
    // LUISA_INFO("Updating AnimNode_SequencePlayer");
    RBCZoneScopedN("AnimNode_SequencePlayer::Update_AnyThread");
    // add asset player to synchronizer
    // BlendWeight = InContext.GetFinalBlendWeight()
    UpdateAssetPlayer(InContext);
}

void AnimNode_SequencePlayer::Evaluate_AnyThread(PoseContext &Output) {
    // LUISA_INFO("Evaluating AnimNode_SequencePlayer with time {}", internal_time_accumulator);

    // RBCZoneScopedN("AnimNode_SequencePlayer::Evaluate_AnyThread");
    // if (!anim_seq_resource) {
    //     Output.ResetToRefPose();
    // }
    // AnimSequenceResource *anim_seq = anim_seq_resource.get();
    // if (anim_seq != nullptr) {
    //     AnimationPoseData pose_data{Output};
    //     AnimExtractContext extract_ctx;
    //     extract_ctx.current_time = internal_time_accumulator;
    //     extract_ctx.delta_time_record = delta_time_record;

    //     anim_seq->animation.GetAnimationPose(pose_data, extract_ctx);
    // } else {
    //     Output.ResetToRefPose();
    // }
}

void AnimNode_SequencePlayer::NodeDebug() {
    // LUISA_INFO("Debugging AnimNode_SequencePlayer");
}

}// namespace rbc

// impl
namespace rbc {

void AnimNode_SequencePlayer::UpdateAssetPlayer(const AnimationUpdateContext &InContext) {
    if (anim_seq_resource) {
        // temp: direct add and loop
        // internal_time_accumulator += InContext.GetDeltaTime();
        // AnimSequenceResource *anim = anim_seq_resource.get();
        // if (internal_time_accumulator > anim->animation.GetAnimation().duration()) {
        //     internal_time_accumulator = 0.0f;
        // }

        // TODO: use TickRecordSystem
        // CreateTickRecordForNode(InContext, anim_seq_resource.get_installed(), is_looping, false);
    }
}
void AnimNode_SequencePlayer::CreateTickRecordForNode(const AnimationUpdateContext &InContext, AnimSequenceResource *InAnimSeqResource, bool bLooping, bool bIsEvaluator) {
    // 假设AnimSeqResource已经install
    // auto tick_record = AnimTickRecord(
    //     &(InAnimSeqResource->animation),
    //     bLooping,
    //     1.0f,
    //     bIsEvaluator,
    //     1.0f,
    //     /**InOut */ internal_time_accumulator);
}

}// namespace rbc