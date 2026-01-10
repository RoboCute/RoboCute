#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/optional.h>
#include <luisa/core/stl/memory.h>
#include <rbc_config.h>
#include <rbc_core/quaternion.h>
#include <rbc_core/type_info.h>
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
    void set_rotation_towards(double3 target_pos);
    void set_rotation_from_direction(double3 dir);
    void set_aspect_ratio_from_resolution(double width, double height);

    // matrix getter
    [[nodiscard]] std::array<double3, 8> frustum_corners(double2 min_projection = double2(-1, -1), double2 max_projection = double2(1, 1)) const;
    [[nodiscard]] std::array<double3, 4> frustum_plane_points(double z_depth, double2 min_projection = double2(-1, -1), double2 max_projection = double2(1, 1)) const;
    [[nodiscard]] std::array<double4, 6> frustum_plane(double2 min_projection = double2(-1, -1), double2 max_projection = double2(1, 1)) const;
    [[nodiscard]] double4x4 rotation_matrix() const;
    [[nodiscard]] double4x4 projection_matrix() const;
    [[nodiscard]] double4x4 local_to_world_matrix() const;
    [[nodiscard]] double4x4 world_to_local_matrix() const;

    // coordinate transform
    [[nodiscard]] double3 local_to_world_position(double3 local_pos) const;
    [[nodiscard]] double3 world_to_local_position(double3 world_pos) const;
    [[nodiscard]] double3 local_to_world_direction(double3 local_dir) const;
    [[nodiscard]] double3 world_to_local_direction(double3 world_dir) const;

    // direction getter
    [[nodiscard]] double3 dir_forward() const;
    [[nodiscard]] double3 dir_right() const;
    [[nodiscard]] double3 dir_up() const;

    ////////// Legacy two point perspective
    // shift & pitch
    void pump_pitch_to_shift();
    void pump_shift_to_pitch();
    double2 shift = {0, 0};
    double2 scale = {1, 1};

    ////////// Legacy two point perspective

    double3 position = double3::zero();
    double3 global_offset = double3::zero();

    double rotation_yaw = 0.0f;
    double rotation_pitch = 0.0f;
    double rotation_roll = 0.0f;
    luisa::optional<Quaternion> rotation_quaternion;

    double fov = radians(60.0f);
    double aspect_ratio = 1;
    double near_plane = 0.3f;
    double far_plane = 20000.0f;
    // physical camera
    bool enable_physical_camera = false;
    double aperture = 1.4f;
    double focus_distance = 2.0f;
};

}// namespace rbc

RBC_RTTI(rbc::Camera)