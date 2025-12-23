#pragma once

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_float.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/geometry/runtime/skinning_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"

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
};

}// namespace rbc

// void skr::Serialize<skr::AnimFloat4x4>::read(skr::ArchiveRead& r, skr::AnimFloat4x4& v)
// {
//     float f[4];
//     for (auto i = 0; i < 4; i++)
//     {
//         r.value<float>(f[0]);
//         r.value<float>(f[1]);
//         r.value<float>(f[2]);
//         r.value<float>(f[3]);
//         v.cols[i] = ozz::math::simd_float4::LoadPtr(f);
//     }
// }
// void skr::Serialize<skr::AnimFloat4x4>::write(skr::ArchiveWrite& w, const skr::AnimFloat4x4& v)
// {
//     float f[4];
//     for (auto i = 0; i < 4; i++)
//     {
//         ozz::math::StorePtr(v.cols[i], f);
//         w.value<float>(f[0]);
//         w.value<float>(f[1]);
//         w.value<float>(f[2]);
//         w.value<float>(f[3]);
//     }
// }