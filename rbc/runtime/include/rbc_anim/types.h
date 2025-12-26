#pragma once

#include "rbc_core/serde.h"

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_float.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
// offline assets
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
// runtime assets
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
// runtime jobs
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/geometry/runtime/skinning_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"

namespace rbc {

using IndexType = int32_t;
constexpr IndexType INVALID_INDEX = -1;
using BoneIndexType = int16_t;

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
// raw asset
using RawAnimationAsset = ozz::animation::offline::RawAnimation;
using RawSkeletonAsset = ozz::animation::offline::RawSkeleton;

}// namespace rbc

// Specialize for AnimFloat4x4 (which is ozz::math::Float4x4)
template<>
struct rbc::Serialize<rbc::AnimFloat4x4> {
    static RBC_RUNTIME_API void write(rbc::ArchiveWrite &w, const rbc::AnimFloat4x4 &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::AnimFloat4x4 &v);
};
