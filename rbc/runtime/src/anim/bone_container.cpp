#include "rbc_anim/bone_container.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc {

BoneContainer::BoneContainer() {}

}// namespace rbc

namespace rbc {

void BoneContainer::Reset() {
    ref_skeleton = nullptr;
    bone_indices.clear();
}

void BoneContainer::InitializeTo(luisa::span<const BoneIndexType> InRequiredBoneIndices, SkeletalMesh *InSkelMesh /*Config Placeholder*/) {
    // copy from required span
    bone_indices.clear();
    bone_indices.insert(
        bone_indices.begin(),
        InRequiredBoneIndices.begin(),
        InRequiredBoneIndices.end());
    asset_skeletal_mesh = InSkelMesh;
    Initialize();
}

const bool BoneContainer::IsValid() const {
    // Asset is Valid
    // RefSkeleton != nullptr
    // BoneIndices.size() > 0
    return (ref_skeleton != nullptr) && (bone_indices.size() > 0);
}

luisa::vector<BoneIndexType> &BoneContainer::GetBoneIndices() {
    return bone_indices;
}
const luisa::vector<BoneIndexType> &BoneContainer::GetBoneIndices() const {
    return bone_indices;
}

void BoneContainer::Initialize() {

    ref_skeleton = &(asset_skeletal_mesh.lock()->GetRefSkeleton());
    // TODO: Init SkeletonToCompactPose and CompactPoseToSkeletonIndex
    // BoneSwitchArrays
    // RemapFromSkelMesh
    // TODO: Setup Compact Poes Data
    // CacheRequiredAnimCurves
}
void BoneContainer::RemapFromSkelMesh() {}
void BoneContainer::RemapFromSkeleton() {}

}// namespace rbc