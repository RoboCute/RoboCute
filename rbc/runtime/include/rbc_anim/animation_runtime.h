#pragma once
#include <rbc_anim/bone_indices.h>
#include <rbc_anim/asset/reference_skeleton.h>

namespace rbc {

struct RBC_RUNTIME_API AnimationRuntime {
    static void EnsureParentsPresent(luisa::vector<BoneIndexType> &InBoneIndices, const ReferenceSkeleton &InRefSkeleton);
};

}// namespace rbc