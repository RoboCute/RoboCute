#pragma once
/**
 * Editor Misc
 * ==================================
 * Miscellaneous utility functions.
 */

#include <luisa/vstl/common.h>
#include <luisa/dsl/builtin.h>

namespace rbc {

// screen coordinate => uv
[[nodiscard]] inline luisa::float2 screen2uv(const luisa::float2 xy, const luisa::float2 resolution) noexcept {
    return clamp(xy / luisa::make_float2(resolution), luisa::float2(0.0f), luisa::float2(1.0f));
}

[[nodiscard]] inline luisa::float2 uv2ndc(const luisa::float2 uv) noexcept {
    return uv * 2.0f - 1.0f;
}

}// namespace rbc