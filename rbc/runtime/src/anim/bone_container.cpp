#include "rbc_anim/bone_container.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc {

BoneContainer::BoneContainer()
    : _ref_skeleton(nullptr) {}

void BoneContainer::reset() {
    _ref_skeleton = nullptr;
    _bone_indices.clear();
}

void BoneContainer::initialize_to(luisa::span<const BoneIndexType> required_bone_indices, SkeletalMesh *skel_mesh /*Config Placeholder*/) {
    // copy from required span
    _bone_indices.assign(required_bone_indices.begin(), required_bone_indices.end());
    _asset_skeletal_mesh = skel_mesh;

    _initialize();
}

bool BoneContainer::is_valid() const {
    // Asset is Valid
    // RefSkeleton != nullptr
    // BoneIndices.size() > 0
    return (_ref_skeleton != nullptr) && (!_bone_indices.empty());
}

luisa::vector<BoneIndexType> &BoneContainer::get_bone_indices() {
    return _bone_indices;
}
const luisa::vector<BoneIndexType> &BoneContainer::get_bone_indices() const {
    return _bone_indices;
}

void BoneContainer::_initialize() {
    if (auto mesh = _asset_skeletal_mesh.lock()) {
        _ref_skeleton = &(mesh->GetRefSkeleton());
    }
    // TODO: Init SkeletonToCompactPose and CompactPoseToSkeletonIndex
    // BoneSwitchArrays
    // RemapFromSkelMesh
    // TODO: Setup Compact Poes Data
    // CacheRequiredAnimCurves
}
void BoneContainer::_remap_from_skel_mesh() {}
void BoneContainer::_remap_from_skeleton() {}

}// namespace rbc
