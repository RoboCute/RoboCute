#include "rbc_anim/graph/AnimNodeContext.h"
#include "rbc_anim/anim_instance.h"

namespace rbc {

AnimInstance *AnimationBaseContext::GetAnimInstanceObject() const { return anim_instance_proxy->GetAnimInstanceObject(); }

AnimationBaseContext::AnimationBaseContext(AnimInstanceProxy *InAnimInstanceProxy, AnimationUpdateSharedContext *InShareContext)
    : anim_instance_proxy(InAnimInstanceProxy), shared_context(InShareContext), current_node_id(INVALID_INDEX), previous_node_id(INVALID_INDEX) {
}

float AnimationUpdateContext::GetDeltaTime() const { return delta_time; }
float AnimationUpdateContext::GetFinalBlendWeight() const { return current_weight; }

void PoseContext::InitializeImpl(AnimInstanceProxy *InAnimInstanceProxy) {
    // SKR_LOG_INFO(u8"PoseContext::Initialize");

    anim_instance_proxy = InAnimInstanceProxy;
    // Require Bones
    // Pose
    const BoneContainer &RequiredBones = anim_instance_proxy->GetRequiredBones();
    Pose.SetBoneContainer(&RequiredBones);
    // Curves
}

void PoseContext::ResetToRefPose() {
    // SKR_LOG_INFO(u8"PoseContext Reset Pose To RefPose");
    Pose.ResetToRefPose();
}

AnimationPoseData::AnimationPoseData(PoseContext &InPoseContext)
    : pose(InPoseContext.Pose) {
}
AnimationPoseData::AnimationPoseData(CompactPose &InPose)
    : pose(InPose) {
}
const CompactPose &AnimationPoseData::GetPose() const {
    return pose;
}
CompactPose &AnimationPoseData::GetPose() {
    return pose;
}

}// namespace rbc