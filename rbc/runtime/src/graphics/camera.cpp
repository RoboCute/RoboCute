#include <rbc_graphics/camera.h>
#include <rbc_graphics/frustum.h>
#include <rbc_core/quaternion.h>

namespace rbc {
// help setter

void Camera::set_aspect_ratio_from_resolution(double width, double height) {
    if (auto_aspect_ratio)
        aspect_ratio = width / height;
}
// matrix getter
double4x4 Camera::rotation_matrix() const {
    auto result = rotation_data.visit_or(double4x4::eye(1), [&]<typename T>(T const &t) {
        if constexpr (std::is_same_v<T, double3x3>) {
            return make_double4x4(
                make_double4(t[0], 0.0),
                make_double4(t[1], 0.0),
                make_double4(t[2], 0.0),
                make_double4(t[3], 1.0));
        } else {
            return rotation(double3(0), t, double3(1));
        }
    });
    result.cols[1] *= -1.0;
    return result;
}
double4x4 Camera::projection_matrix() const {
    auto result = perspective_lh(
        fov,
        aspect_ratio,
        far_plane,
        near_plane);

    return result;
}

// coordinate transform
double4x4 Camera::local_to_world_matrix() const {
    return translation(make_double3(position)) * rotation_matrix();
}
double4x4 Camera::world_to_local_matrix() const {
    return inverse(local_to_world_matrix());
}
double3 Camera::local_to_world_position(double3 local_pos) const {
    return (local_to_world_matrix() * double4(local_pos.x, local_pos.y, local_pos.z, 1.0)).xyz();
}
double3 Camera::world_to_local_position(double3 world_pos) const {
    return (world_to_local_matrix() * double4(world_pos.x, world_pos.y, world_pos.z, 1.0)).xyz();
}
double3 Camera::local_to_world_direction(double3 local_dir) const {
    return (local_to_world_matrix() * double4(local_dir.x, local_dir.y, local_dir.z, 0.0)).xyz();
}
double3 Camera::world_to_local_direction(double3 world_dir) const {
    return (world_to_local_matrix() * double4(world_dir.x, world_dir.y, world_dir.z, 0.0)).xyz();
}

// direction getter
double3 Camera::dir_forward() const {
    return local_to_world_direction(double3(0, 0, 1));
}
double3 Camera::dir_right() const {
    return local_to_world_direction(double3(1, 0, 0));
}
double3 Camera::dir_up() const {
    return local_to_world_direction(double3(0, 1, 0));
}

std::array<double3, 4> Camera::frustum_plane_points(double z_depth, double2 min_projection, double2 max_projection) const {
    std::array<double3, 4> corners;
    double up_length = z_depth * tan(this->fov * 0.5);
    double right_length = up_length * aspect_ratio;
    double3 far_point = make_double3(position) + z_depth * dir_forward();
    double3 up_vec = up_length * dir_up();
    double3 right_vec = -right_length * dir_right();
    corners[0] = far_point + up_vec * min_projection.y + right_vec * min_projection.x;
    corners[1] = far_point + up_vec * min_projection.y + right_vec * max_projection.x;
    corners[2] = far_point + up_vec * max_projection.y + right_vec * min_projection.x;
    corners[3] = far_point + up_vec * max_projection.y + right_vec * max_projection.x;
    return corners;
}

std::array<double3, 8> Camera::frustum_corners(double2 min_projection, double2 max_projection) const {
    std::array<double3, 8> corners;
    double fov_tan = tan(this->fov * 0.5);
    double3 forward = dir_forward();
    double3 up = dir_up();
    double3 right = dir_right();
    auto get_corner = [&](double3 *out_corners, double dist, double3 cam_pos, double aspect) {
        double up_length = dist * fov_tan;
        double right_length = up_length * aspect;
        double3 far_point = cam_pos + dist * forward;
        double3 up_vec = up_length * up;
        double3 right_vec = -right_length * right;
        out_corners[0] = far_point + up_vec * min_projection.y + right_vec * min_projection.x;
        out_corners[1] = far_point + up_vec * min_projection.y + right_vec * max_projection.x;
        out_corners[2] = far_point + up_vec * max_projection.y + right_vec * min_projection.x;
        out_corners[3] = far_point + up_vec * max_projection.y + right_vec * max_projection.x;
    };

    get_corner(
        corners.data(),
        near_plane,
        make_double3(position),
        aspect_ratio);
    get_corner(
        corners.data() + 4,
        far_plane,
        make_double3(position),
        aspect_ratio);
    return corners;
}

std::array<double4, 6> Camera::frustum_plane(double2 min_projection, double2 max_projection) const {
    std::array<double4, 6> planes;
    auto corners = frustum_plane_points(far_plane, min_projection, max_projection);
    auto cam_position = make_double3(this->position);
    double3 forward = dir_forward();
    planes[0] = get_plane(corners[1], corners[0], cam_position);
    planes[1] = get_plane(corners[2], corners[3], cam_position);
    planes[2] = get_plane(corners[0], corners[2], cam_position);
    planes[3] = get_plane(corners[3], corners[1], cam_position);
    planes[4] = get_plane(forward, cam_position + forward * far_plane);
    planes[5] = get_plane(-forward, cam_position + forward * near_plane);
    return planes;
}

RBC_RUNTIME_API bool frustum_cull(
    luisa::double4x4 const &local_to_world,
    luisa::compute::AABB const &bounding,
    luisa::span<luisa::double4 const, 6> frustum_planes,
    luisa::double3 frustum_min_point,
    luisa::double3 frustum_max_point,
    luisa::double3 cam_forward,
    luisa::double3 cam_pos) {
    double3 global_min{1e20};
    double3 global_max{-1e20};
    double3 const local_min{bounding.packed_min[0], bounding.packed_min[1], bounding.packed_min[2]};
    double3 const local_max{bounding.packed_max[0], bounding.packed_max[1], bounding.packed_max[2]};
    for (int i = 0; i < 8; ++i) {
        double3 point = select(local_min, local_max, int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) == 1);
        point = (local_to_world * make_double4(point, 1.0)).xyz();
        global_min = min(global_min, point);
        global_max = max(global_max, point);
    }
    double3 const position = lerp(global_min, global_max, 0.5);
    double3 const extent = global_max - position;
    if (any(global_min > frustum_max_point) || any(global_max < frustum_min_point)) {
        return false;
    }
    for (auto const &plane : frustum_planes) {
        double3 const abs_normal = abs(plane.xyz());
        if ((dot(position, plane.xyz()) - dot(abs_normal, extent)) > -plane.w) {
            return false;
        }
    }
    return true;
}

}// namespace rbc