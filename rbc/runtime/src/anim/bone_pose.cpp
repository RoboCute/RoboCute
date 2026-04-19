#include "rbc_anim/bone_pose.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include "rbc_anim/bone_container.h"

namespace rbc {

void BaseCompactPose::reset_to_ref_pose() {
    reset_to_ref_pose(get_bone_container());
}
void BaseCompactPose::reset_to_ref_pose(const BoneContainer &required_bones) {
    // RequiredBones.fill_with_compact_ref_pose(this->Bones);
    _bone_container = &required_bones;
    const luisa::span<const AnimSOATransform> ref_pose = _bone_container->get_reference_skeleton()->joint_rest_poses();

    for (auto bone_index = 0; bone_index < ref_pose.size(); bone_index++) {
        // const int32_t skel_bone_index = _bone_container->GetSkelBoneIndex(bone_index);
        // this->_bones[bone_index] = ref_pose[skel_bone_index];
        this->_bones[bone_index] = ref_pose[bone_index];
    }
}

void BaseCompactPose::clear() {
    _bone_container = nullptr;
    _bones.clear();
}

bool BaseCompactPose::is_valid() const {
    return true;
}

bool BaseCompactPose::is_normalized() const {
    (void)_bones;
    // for (const auto &bone : _bones) {
    //     if (!bone.IsRotationNormalized()) { return false;}
    // }
    return true;
}

BoneContainer &BaseCompactPose::get_bone_container() {
    // CheckSlow(Valid)
    return *const_cast<BoneContainer *>(_bone_container);
}

const BoneContainer &BaseCompactPose::get_bone_container() const {
    // CheckSlow(Valid)
    return *_bone_container;
}

void BaseCompactPose::set_bone_container(const BoneContainer *bone_container) {
    // CheckSlow
    _bone_container = bone_container;

    this->init_bones(static_cast<int>(_bone_container->get_bone_indices().size()));
}

}// namespace rbc

// Compact Pose Impl
namespace rbc {

void CompactPose::reset_to_additive_identity() {}
void CompactPose::normalize_rotation() {
    for (auto &bone : _bones) {
        bone.rotation = ozz::math::Normalize(bone.rotation);
    }
}

}// namespace rbc