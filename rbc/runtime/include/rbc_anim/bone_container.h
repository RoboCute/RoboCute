#pragma once
#include "rbc_anim/types.h"
#include "rbc_anim/bone_indices.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include <rbc_core/rc.h>

// Native Transient Container for Bones

namespace rbc {
struct SkeletalMesh;
}// namespace rbc

namespace rbc {

struct BoneContainer {

private:
    luisa::vector<BoneIndexType> bone_indices;
    RCWeak<SkeletalMesh> asset_skeletal_mesh;
    // Lookup Tables
    luisa::vector<int32_t> CompactPoseToSkeletonIndex;
    luisa::vector<int32_t> SkeletonToCompactPose;
    ReferenceSkeleton *ref_skeleton;// the real referenced animation

public:
    BoneContainer();

public:
    void Reset();
    void InitializeTo(luisa::span<const BoneIndexType> InRequiredBoneIndices, SkeletalMesh *InSkelMesh /*Config Placeholder*/);

    const bool IsValid() const;
    luisa::vector<BoneIndexType> &GetBoneIndices();
    const luisa::vector<AnimSOATransform> &GetRefPoses();
    const luisa::vector<BoneIndexType> &GetBoneIndices() const;
    const ReferenceSkeleton *GetReferenceSkeleton() const { return ref_skeleton; }

    const int32_t GetNumBones() const {
        return ref_skeleton->NumJoints();
    }

    const int32_t GetCompactPoseNumBones() const {
        return bone_indices.size();
    }

public:
    template<typename ArrayType>
    void FillWithCompactRefPose(ArrayType &OutTransform) const {
        const int32_t compact_pose_count = GetCompactPoseNumBones();
        // OutTransform.Reset()
        // OutTransform.SetNumUnInitialized();
        // override
        const auto &ref_pose = ref_skeleton->JointRestPoses();
        for (int32_t c_idx = 0; c_idx < compact_pose_count; ++c_idx) {
            OutTransform[c_idx] = ref_pose[bone_indices[c_idx]];
        }
    }

private:
    // Core Methods
    void Initialize();
    void RemapFromSkelMesh();
    void RemapFromSkeleton();
};

}// namespace rbc