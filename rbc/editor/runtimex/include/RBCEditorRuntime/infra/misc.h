#pragma once
/**
 * Editor Misc
 * ==================================
 * 一些不知道该放在哪的功能函数
 */

#include <luisa/vstl/common.h>
#include <luisa/dsl/builtin.h>

namespace rbc {

// screen coordinate => uv
inline luisa::float2 screen2uv(luisa::float2 xy, luisa::float2 resolution) {
    return clamp(xy / luisa::make_float2(resolution), luisa::float2(0.0f), luisa::float2(1.0f));
}

inline luisa::float2 uv2ndc(luisa::float2 uv) {
    return uv * 2.0f - 1.0f;
}

}// namespace rbc