#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/optional.h>
#include <luisa/core/stl/memory.h>
#include <rbc_config.h>
#include <rbc_core/quaternion.h>
#include <rbc_graphics/matrix.h>
namespace rbc {
using namespace luisa;

enum struct TwoPointPerspectiveType : int {
    None,
    Shift,
    Legacy
};

// clang-format off
struct RBC_RUNTIME_API Camera {
    // clang-format on
    // help setter
    void set_rotation_towards(float3 target_pos);
    void set_rotation_from_direction(float3 dir);
    void set_aspect_ratio_from_resolution(float width, float height);

    // matrix getter
    std::array<float3, 8> frustum_corners(float2 min_projection = float2(-1, -1), float2 max_projection = float2(1, 1)) const;
    std::array<float3, 4> frustum_plane_points(float z_depth, float2 min_projection = float2(-1, -1), float2 max_projection = float2(1, 1)) const;
    std::array<float4, 6> frustum_plane(float2 min_projection = float2(-1, -1), float2 max_projection = float2(1, 1)) const;
    float4x4 rotation_matrix() const;
    float4x4 projection_matrix() const;
    float4x4 local_to_world_matrix() const;
    float4x4 world_to_local_matrix() const;

    // coordinate transform
    float3 local_to_world_position(float3 local_pos) const;
    float3 world_to_local_position(float3 world_pos) const;
    float3 local_to_world_direction(float3 local_dir) const;
    float3 world_to_local_direction(float3 world_dir) const;

    // direction getter
    float3 dir_forward() const;
    float3 dir_right() const;
    float3 dir_up() const;

    ////////// Legacy two point perspective
    // shift & pitch
    void pump_pitch_to_shift();
    void pump_shift_to_pitch();
    float2 shift = {0, 0};
    float2 scale = {1, 1};

    ////////// Legacy two point perspective

    double3 position = double3::zero();
    double3 global_offset = double3::zero();

    float rotation_yaw = 0.0f;
    float rotation_pitch = 0.0f;
    float rotation_roll = 0.0f;
    luisa::optional<Quaternion> rotation_quaternion;

    float fov = radians(60.0f);
    float aspect_ratio = 1;
    float near_plane = 0.3f;
    float far_plane = 20000.0f;
    // physical camera
    bool enable_physical_camera = false;
    float aperture = 1.4f;
    float focus_distance = 2.0f;
};

using CameraComponent = Camera;
}// namespace rbc
