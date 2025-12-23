#pragma once

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_float.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/animation.h"
#include "ozz/skeleton.h"
#include "ozz/geometry/skinning_job.h"
#include "ozz/local_to_model_job.h"
#include "ozz/sampling_job.h"

namespace rbc {

using IndexType = int32_t;
constexpr IndexType INVALID_INDEX = -1;

using AnimSOATransform = ozz::math::SoaTransform;
using AnimSOAFloat3 = ozz::math::SoaFloat3;
using AnimSoaQuaternion = ozz::math::SoaQuaternion;
using AnimFloat4x4 = ozz::math::Float4x4;

using AnimSequenceRuntimeAsset = ozz::animation::Animation;
using SkeletonRuntimeAsset = ozz::animation::Skeleton;

using AnimSkinningJob = ozz::geometry::SkinningJob;
using AnimLocalToModelJob = ozz::animation::LocalToModelJob;
using AnimSamplingJob = ozz::animation::SamplingJob;
using AnimSamplingJobContext = ozz::animation::SamplingJob::Context;

enum class ERootMotionMode {
    NoRootMotionExtraction,
    IgnoreRootMotion,
    RootMotionFromEverything,
    // RootMotionFromMontageOnly
};

}// namespace rbc