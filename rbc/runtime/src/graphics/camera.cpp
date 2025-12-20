#include <rbc_graphics/camera.h>
#include <rbc_graphics/frustum.h>

namespace rbc {
// help setter
void Camera::set_rotation_towards(double3 target_pos) {
    set_rotation_from_direction(make_double3(make_double3(target_pos) - position));
}
void Camera::set_rotation_from_direction(double3 dir) {
    dir = normalize(dir);
    rotation_pitch = asin(dir.y);
    rotation_yaw = atan2f(-dir.x, dir.z);
}
void Camera::set_aspect_ratio_from_resolution(double width, double height) {
    aspect_ratio = width / height;
}
// matrix getter
double4x4 Camera::rotation_matrix() const {
    auto result = [&] {
        if (rotation_quaternion.has_value()) {
            return rotation(double3(0), rotation_quaternion.value(), double3(1));
        } else {
            auto result = transpose(
                rotation(double3(0, 0, 1), rotation_roll) *
                rotation(double3(1, 0, 0), rotation_pitch) *
                rotation(double3(0, 1, 0), rotation_yaw));
            return result;
        }
    }();
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

// shift & pitch
void Camera::pump_pitch_to_shift() {
    double vertical_fov = fov / aspect_ratio;
    double factor = (pi / 4) / (vertical_fov / 2);
    double pitch_from_shift = atan(shift.y / factor);

    shift.y = tan(pitch_from_shift + rotation_pitch) * factor;
    rotation_pitch = 0.0f;
}
void Camera::pump_shift_to_pitch() {
    double vertical_fov = fov / aspect_ratio;
    double factor = (pi / 4) / (vertical_fov / 2);
    double pitch_from_shift = atan(shift.y / factor);

    rotation_pitch += pitch_from_shift;
    shift.y = 0.0f;
}

// coordinate transform
double4x4 Camera::local_to_world_matrix() const {
    return translation(make_double3(position)) * rotation_matrix();
}
double4x4 Camera::world_to_local_matrix() const {
    return inverse(local_to_world_matrix());
}
double3 Camera::local_to_world_position(double3 local_pos) const {
    return (local_to_world_matrix() * double4(local_pos.x, local_pos.y, local_pos.z, 1.0f)).xyz();
}
double3 Camera::world_to_local_position(double3 world_pos) const {
    return (world_to_local_matrix() * double4(world_pos.x, world_pos.y, world_pos.z, 1.0f)).xyz();
}
double3 Camera::local_to_world_direction(double3 local_dir) const {
    return (local_to_world_matrix() * double4(local_dir.x, local_dir.y, local_dir.z, 0.0f)).xyz();
}
double3 Camera::world_to_local_direction(double3 world_dir) const {
    return (world_to_local_matrix() * double4(world_dir.x, world_dir.y, world_dir.z, 0.0f)).xyz();
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
    double upLength = z_depth * tan(this->fov * 0.5);
    double rightLength = upLength * aspect_ratio;
    double3 farPoint = make_double3(position) + z_depth * dir_forward();
    double3 upVec = upLength * dir_up();
    double3 rightVec = -rightLength * dir_right();
    corners[0] = farPoint + upVec * min_projection.y + rightVec * min_projection.x;
    corners[1] = farPoint + upVec * min_projection.y + rightVec * max_projection.x;
    corners[2] = farPoint + upVec * max_projection.y + rightVec * min_projection.x;
    corners[3] = farPoint + upVec * max_projection.y + rightVec * max_projection.x;
    return corners;
}

std::array<double3, 8> Camera::frustum_corners(double2 min_projection, double2 max_projection) const {
    std::array<double3, 8> corners;
    double fov = tan(this->fov * 0.5);
    double3 forward = dir_forward();
    double3 up = dir_up();
    double3 right = dir_right();
    auto get_corner = [&](double3 *corners, double dist, double3 position, double aspect) {
        double upLength = dist * (fov);
        double rightLength = upLength * aspect;
        double3 farPoint = position + dist * forward;
        double3 upVec = upLength * up;
        double3 rightVec = -rightLength * right;
        corners[0] = farPoint + upVec * min_projection.y + rightVec * min_projection.x;
        corners[1] = farPoint + upVec * min_projection.y + rightVec * max_projection.x;
        corners[2] = farPoint + upVec * max_projection.y + rightVec * min_projection.x;
        corners[3] = farPoint + upVec * max_projection.y + rightVec * max_projection.x;
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
    auto position = make_double3(this->position);
    double3 forward = dir_forward();
    planes[0] = get_plane(corners[1], corners[0], position);
    planes[1] = get_plane(corners[2], corners[3], position);
    planes[2] = get_plane(corners[0], corners[2], position);
    planes[3] = get_plane(corners[3], corners[1], position);
    planes[4] = get_plane(forward, position + forward * far_plane);
    planes[5] = get_plane(-forward, position + forward * near_plane);
    return planes;
}

RBC_RUNTIME_API bool frustum_cull(
    luisa::double4x4 const &local_to_world,
    luisa::compute::AABB const &bounding,
    luisa::span<luisa::double4, 6> frustum_planes,
    luisa::double3 frustum_min_point,
    luisa::double3 frustum_max_point,
    luisa::double3 cam_forward,
    luisa::double3 cam_pos) {
    double3 global_min{1e20f};
    double3 global_max{-1e20f};
    double3 local_min{bounding.packed_min[0], bounding.packed_min[1], bounding.packed_min[2]};
    double3 local_max{bounding.packed_max[0], bounding.packed_max[1], bounding.packed_max[2]};
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
            for (int z = 0; z < 2; z++) {
                double3 point = select(local_min, local_max, int3(x, y, z) == 1);
                point = (local_to_world * make_double4(point, 1.f)).xyz();
                global_min = min(global_min, point);
                global_max = max(global_max, point);
            }
    double3 position = lerp(global_min, global_max, 0.5);
    double3 extent = global_max - position;
    if (any(global_min > frustum_max_point) || any(global_max < frustum_min_point)) {
        return false;
    }
    for (auto &plane : frustum_planes) {
        double3 abs_normal = abs(plane.xyz());
        if ((dot(position, plane.xyz()) - dot(abs_normal, extent)) > -plane.w) {
            return false;
        }
    }
    return true;
}

}// namespace rbc