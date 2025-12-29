#include "rbc_anim/animation_runtime.h"

namespace rbc {

void AnimationRuntime::EnsureParentsPresent(luisa::vector<BoneIndexType> &InBoneIndices, const ReferenceSkeleton &InRefSkeleton) {
    InRefSkeleton.EnsureParentsExist(InBoneIndices);
}

}// namespace rbc