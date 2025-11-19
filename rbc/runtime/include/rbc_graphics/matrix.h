#pragma once
#include <rbc_config.h>
#include <luisa/core/mathematics.h>

RBC_RUNTIME_API luisa::float4x4 perspective_lh(
    float fov_angle_y,
    float aspect_ratio,
    float near_z,
    float far_z
);
RBC_RUNTIME_API luisa::float4 combine_cone(luisa::float4 cone_0, luisa::float4 cone_1);

template <typename T>
requires(luisa::is_vector_v<T, 3> && std::is_floating_point_v<luisa::vector_element_t<T>>)
luisa::Vector<luisa::vector_element_t<T>, 4> get_plane(T normal, T in_point)
{
    auto d = -dot(normal, in_point);
    return luisa::Vector<luisa::vector_element_t<T>, 4>{
        normal.x, normal.y, normal.z, d
    };
}

template <typename T>
requires(luisa::is_vector_v<T, 3> && std::is_floating_point_v<luisa::vector_element_t<T>>)
luisa::Vector<luisa::vector_element_t<T>, 4> get_plane(T a, T b, T c)
{
    auto normal = normalize(cross(b - a, c - a));
    return get_plane(normal, a);
}

template <typename T>
requires(std::is_floating_point_v<T>)
T distance_to_plane(luisa::Vector<T, 4> plane, luisa::Vector<T, 3> point)
{
    return dot(plane.xyz, point) + plane.w;
}