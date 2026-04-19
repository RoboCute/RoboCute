#include "rbc_anim/animation_runtime.h"

namespace rbc {

void AnimationRuntime::ensure_parents_present(luisa::vector<BoneIndexType> &in_bone_indices, const ReferenceSkeleton &in_ref_skeleton) {
    in_ref_skeleton.ensure_parents_exist(in_bone_indices);
}

}// namespace rbc