#pragma once

#include <fsd/constants.hpp>
#include <geometry/types.hpp>
#include <luisa/std.hpp>
#include <utils/heap_indices.hpp>

namespace fsd {
using namespace luisa::shader;

// For a unit direction, this maximizes the minimum absolute axis component.
constexpr float query_ray_direction_component = 0.5773502691896258f;

inline uint edge_hash(uint begin, uint end) {
    auto value = begin * 0x9e3779b9u ^ end * 0x85ebca6bu;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    return value;
}

inline uint2 triangle_edge(
    geometry::Triangle triangle,
    uint edge_index) {
    uint2 edge;
    if (edge_index == 0u) {
        edge = uint2(triangle[0], triangle[1]);
    } else if (edge_index == 1u) {
        edge = uint2(triangle[0], triangle[2]);
    } else {
        edge = uint2(triangle[1], triangle[2]);
    }
    return uint2(min(edge.x, edge.y), max(edge.x, edge.y));
}

inline float point_segment_distance_squared(float3 point, float3 begin, float3 end) {
    auto edge = end - begin;
    auto edge_length_squared = dot(edge, edge);
    if (edge_length_squared == 0.0f) {
        auto offset = point - begin;
        return dot(offset, offset);
    }

    auto projection = dot(point - begin, edge) / edge_length_squared;
    projection = clamp(projection, 0.0f, 1.0f);
    auto offset = point - (begin + projection * edge);
    return dot(offset, offset);
}

inline float point_triangle_distance_squared(
    float3 point,
    float3 vertex_a,
    float3 vertex_b,
    float3 vertex_c) {
    auto edge_ab = vertex_b - vertex_a;
    auto edge_ac = vertex_c - vertex_a;
    auto normal = cross(edge_ab, edge_ac);
    auto area_squared = dot(normal, normal);
    if (area_squared == 0.0f) {
        return min(
            point_segment_distance_squared(point, vertex_a, vertex_b),
            min(
                point_segment_distance_squared(point, vertex_b, vertex_c),
                point_segment_distance_squared(point, vertex_c, vertex_a)));
    }

    auto signed_plane_distance = dot(point - vertex_a, normal);
    auto projected = point - normal * (signed_plane_distance / area_squared);
    auto edge_bc = vertex_c - vertex_b;
    auto edge_ca = vertex_a - vertex_c;
    auto inside_ab = dot(cross(edge_ab, projected - vertex_a), normal);
    auto inside_bc = dot(cross(edge_bc, projected - vertex_b), normal);
    auto inside_ca = dot(cross(edge_ca, projected - vertex_c), normal);
    if (inside_ab >= 0.0f && inside_bc >= 0.0f && inside_ca >= 0.0f) {
        return signed_plane_distance * signed_plane_distance / area_squared;
    }

    return min(
        point_segment_distance_squared(point, vertex_a, vertex_b),
        min(
            point_segment_distance_squared(point, vertex_b, vertex_c),
            point_segment_distance_squared(point, vertex_c, vertex_a)));
}

inline std::array<float3, 3> read_world_triangle(
    BindlessBuffer &heap,
    Accel &sidecar,
    uint2 triangle_ref) {
    auto user_id = sidecar.instance_user_id(triangle_ref.x);
    auto instance_info = heap.uniform_idx_buffer_read<geometry::InstanceInfo>(
        heap_indices::inst_buffer_heap_idx,
        user_id);
    auto mesh = instance_info.mesh;
    auto triangle = heap.byte_buffer_read<geometry::Triangle>(
        mesh.heap_idx,
        mesh.tri_byte_offset + sizeof(geometry::Triangle) * triangle_ref.y);
    auto transform = sidecar.instance_transform(triangle_ref.x);

    std::array<float3, 3> vertices;
    for (uint i = 0u; i < 3u; ++i) {
        auto local_position = heap.buffer_read<float3>(mesh.heap_idx, triangle[i]);
        vertices[i] = (transform * float4(local_position, 1.0f)).xyz;
    }
    return vertices;
}

inline Ray make_query_ray(float3 point, float half_length) {
    auto ray_direction = float3(query_ray_direction_component);
    return Ray(
        point - ray_direction * half_length,
        ray_direction,
        0.0f,
        2.0f * half_length);
}

inline bool triangle_is_in_radius(
    BindlessBuffer &heap,
    Accel &sidecar,
    uint2 triangle_ref,
    float3 point,
    float radius) {
    auto vertices = read_world_triangle(heap, sidecar, triangle_ref);
    auto distance_squared = point_triangle_distance_squared(
        point,
        vertices[0],
        vertices[1],
        vertices[2]);
    auto radius_squared = radius * radius;
    return distance_squared <= radius_squared;
}

}// namespace fsd
