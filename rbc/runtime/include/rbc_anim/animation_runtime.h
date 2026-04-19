#pragma once
#include <rbc_anim/bone_indices.h>
#include <rbc_anim/asset/reference_skeleton.h>

namespace rbc {

struct RBC_RUNTIME_API AnimationRuntime {
    static void ensure_parents_present(luisa::vector<BoneIndexType> &in_bone_indices, const ReferenceSkeleton &in_ref_skeleton);
};

}// namespace rbc