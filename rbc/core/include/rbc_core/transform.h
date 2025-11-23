#pragma once

#include <luisa/core/mathematics.h>
#include "rbc_core/quaternion.h"

namespace rbc {

// TODO: 这个数据可能需要跨越python/cpp的结构，是否需要生成？
struct Transform3D {
    using float3 = luisa::float3;
    using float4x4 = luisa::float4x4;

    alignas(16) Quaternion rotation;
    alignas(16) float3 translation;
    alignas(16) float3 scaling;

public:
    // ctor & dtor
    inline Transform3D() : rotation(), translation(), scaling(1) {}
    Transform3D(Quaternion r, float3 t, float3 s) : rotation(r), translation(t), scaling(s) {}
    Transform3D(const Transform3D &rhs) = default;

    inline static Transform3D Identity() {
        return {Quaternion::Identity(), {}, {}};
    }
};

}// namespace rbc