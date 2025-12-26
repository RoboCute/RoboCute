#include "rbc_anim/bone_pose.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include "rbc_anim/bone_container.h"

namespace rbc {

void BaseCompactPose::ResetToRefPose() {
    ResetToRefPose(GetBoneContainer());
}
void BaseCompactPose::ResetToRefPose(const BoneContainer &InRequireBones) {
    // RequiredBones.FillWithCompactRefPose(this->Bones);
    bone_container = &InRequireBones;
    const luisa::span<const AnimSOATransform> ref_pose = bone_container->GetReferenceSkeleton()->JointRestPoses();

    for (auto bone_index = 0; bone_index < ref_pose.size(); bone_index++) {
        // const int32_t skel_bone_index = bone_container->GetSkelBoneIndex(bone_index);
        // this->bones[bone_index] = ref_pose[skel_bone_index];
        this->bones[bone_index] = ref_pose[bone_index];
    }
}

void BaseCompactPose::Clear() {
    bone_container = nullptr;
    bones.clear();
}

bool BaseCompactPose::IsValid() const {
    return true;
}

bool BaseCompactPose::IsNormalized() const {
    for (const auto &bone : bones) {
        // if (!bone.IsRotationNormalized()) { return false;}
    }
    return true;
}

BoneContainer &BaseCompactPose::GetBoneContainer() {
    // CheckSlow(Valid)
    return *const_cast<BoneContainer *>(bone_container);
}

const BoneContainer &BaseCompactPose::GetBoneContainer() const {
    // CheckSlow(Valid)
    return *const_cast<BoneContainer *>(bone_container);
}

void BaseCompactPose::SetBoneContainer(const BoneContainer *InBoneContainer) {
    // CheckSlow
    bone_container = InBoneContainer;

    this->InitBones(bone_container->GetBoneIndices().size());
}

}// namespace rbc

// Compact Pose Impl
namespace rbc {

void CompactPose::ResetToAdditiveIdentity() {}
void CompactPose::NormalizeRotation() {
    for (auto &bone : bones) {
        bone.rotation = ozz::math::Normalize(bone.rotation);
    }
}

}// namespace rbc