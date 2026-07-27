#pragma once

#include <luisa/std.hpp>

namespace lighting {

using namespace luisa::shader;

struct LightSample {
    float3 wi;
    float3 L;
    float pdf;
    float mis_weight;
    float wi_length;
};

namespace detail {

inline float ray_intersect_sphere(
    float3 direction,
    float3 origin,
    float3 center,
    float radius) {
    float a =
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z;
    float b =
        2 * direction.x * (origin.x - center.x) +
        2 * direction.y * (origin.y - center.y) +
        2 * direction.z * (origin.z - center.z);
    float c =
        (origin.x - center.x) * (origin.x - center.x) +
        (origin.y - center.y) * (origin.y - center.y) +
        (origin.z - center.z) * (origin.z - center.z) -
        radius * radius;
    float delta = b * b - 4 * a * c;
    if (delta < 0) {
        return -1.0f;
    }
    if (delta < 1e-4f) {
        return -b / (2 * a);
    }
    return min(
        (-b + sqrt(delta)) / (2 * a),
        (-b - sqrt(delta)) / (2 * a));
}

}// namespace detail

}// namespace lighting
