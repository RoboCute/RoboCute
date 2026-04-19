#include "rbc_anim/graph/AnimNodeContext.h"
#include "rbc_anim/anim_instance.h"
namespace rbc {

AnimInstance *AnimationBaseContext::get_anim_instance_object() const { return anim_instance_proxy->GetAnimInstanceObject(); }

AnimationBaseContext::AnimationBaseContext(AnimInstanceProxy *in_anim_instance_proxy, AnimationUpdateSharedContext *in_share_context)
    : anim_instance_proxy(in_anim_instance_proxy), shared_context(in_share_context), _current_node_id(INVALID_INDEX), _previous_node_id(INVALID_INDEX) {
}

float AnimationUpdateContext::get_delta_time() const { return _delta_time; }
float AnimationUpdateContext::get_final_blend_weight() const { return _current_weight; }

void PoseContext::initialize_impl(AnimInstanceProxy *in_anim_instance_proxy) {
    // SKR_LOG_INFO(u8"PoseContext::Initialize");

    anim_instance_proxy = in_anim_instance_proxy;
    // Require Bones
    // Pose
    const BoneContainer &required_bones = anim_instance_proxy->GetRequiredBones();
    pose.set_bone_container(&required_bones);
    // Curves
}

void PoseContext::reset_to_ref_pose() {
    // SKR_LOG_INFO(u8"PoseContext Reset Pose To RefPose");
    pose.reset_to_ref_pose();
}

AnimationPoseData::AnimationPoseData(PoseContext &in_pose_context)
    : _pose(in_pose_context.pose) {
}
AnimationPoseData::AnimationPoseData(CompactPose &in_pose)
    : _pose(in_pose) {
}
const CompactPose &AnimationPoseData::get_pose() const {
    return _pose;
}
CompactPose &AnimationPoseData::get_pose() {
    return _pose;
}

}// namespace rbc