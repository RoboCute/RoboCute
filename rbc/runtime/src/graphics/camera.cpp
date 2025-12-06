#include <rbc_graphics/camera.h>
#include <rbc_graphics/frustum.h>

namespace rbc {
// help setter
void Camera::set_rotation_towards(float3 target_pos) {
    set_rotation_from_direction(make_float3(make_double3(target_pos) - position));
}
void Camera::set_rotation_from_direction(float3 dir) {
    dir = normalize(dir);
    rotation_pitch = asin(dir.y);
    rotation_yaw = atan2f(-dir.x, dir.z);
}
void Camera::set_aspect_ratio_from_resolution(float width, float height) {
    aspect_ratio = width / height;
}
// matrix getter
float4x4 Camera::rotation_matrix() const {
    auto result = [&] {
        if (rotation_quaternion.has_value()) {
            return rotation(float3(0), rotation_quaternion.value(), float3(1));
        } else {
            auto result = transpose(
                rotation(float3(0, 0, 1), rotation_roll) *
                rotation(float3(1, 0, 0), rotation_pitch) *
                rotation(float3(0, 1, 0), rotation_yaw));
            return result;
        }
    }();
    result.cols[1] *= -1.0f;
    return result;
}
float4x4 Camera::projection_matrix() const {
    auto result = perspective_lh(
        fov,
        aspect_ratio,
        far_plane,
        near_plane);

    if (two_point_perspective_type == TwoPointPerspectiveType::Shift) {
        float2 shift = float2(two_point_perspective_shift.x * 2, -two_point_perspective_shift.y * 2);
        float scale = two_point_perspective_scale;
        float2 mouse_offset0 = float2((two_point_perspective_mouse_origin_position.x - 0.5) * 2, (0.5 - two_point_perspective_mouse_origin_position.y) * 2);
        float2 mouse_offset1 = float2((two_point_perspective_mouse_screen_position.x - 0.5) * 2, (0.5 - two_point_perspective_mouse_screen_position.y) * 2);

        float4x4 translate_scale;
        translate_scale[0][0] = scale;
        translate_scale[1][1] = scale;
        translate_scale[3][0] = (shift.x - mouse_offset0.x) * scale + mouse_offset1.x;
        translate_scale[3][1] = (shift.y - mouse_offset0.y) * scale + mouse_offset1.y;

        result = translate_scale * result;// 注意矩阵相乘顺序，这里是行主导但右乘
    } else if (two_point_perspective_type == TwoPointPerspectiveType::Legacy) {
        result[2][0] = shift.x;
        result[2][1] = shift.y;
        result[0][0] *= scale.x;
        result[1][1] *= scale.y;
    }

    return result;
}

// shift & pitch
void Camera::pump_pitch_to_shift() {
    float vertical_fov = fov / aspect_ratio;
    float factor = (pi / 4) / (vertical_fov / 2);
    float pitch_from_shift = atan(shift.y / factor);

    shift.y = tan(pitch_from_shift + rotation_pitch) * factor;
    rotation_pitch = 0.0f;
}
void Camera::pump_shift_to_pitch() {
    float vertical_fov = fov / aspect_ratio;
    float factor = (pi / 4) / (vertical_fov / 2);
    float pitch_from_shift = atan(shift.y / factor);

    rotation_pitch += pitch_from_shift;
    shift.y = 0.0f;
}

// coordinate transform
float4x4 Camera::local_to_world_matrix() const {
    return translation(make_float3(position)) * rotation_matrix();
}
float4x4 Camera::world_to_local_matrix() const {
    return inverse(local_to_world_matrix());
}
float3 Camera::local_to_world_position(float3 local_pos) const {
    return (local_to_world_matrix() * float4(local_pos.x, local_pos.y, local_pos.z, 1.0f)).xyz();
}
float3 Camera::world_to_local_position(float3 world_pos) const {
    return (world_to_local_matrix() * float4(world_pos.x, world_pos.y, world_pos.z, 1.0f)).xyz();
}
float3 Camera::local_to_world_direction(float3 local_dir) const {
    return (local_to_world_matrix() * float4(local_dir.x, local_dir.y, local_dir.z, 0.0f)).xyz();
}
float3 Camera::world_to_local_direction(float3 world_dir) const {
    return (world_to_local_matrix() * float4(world_dir.x, world_dir.y, world_dir.z, 0.0f)).xyz();
}

// direction getter
float3 Camera::dir_forward() const {
    return local_to_world_direction(float3(0, 0, 1));
}
float3 Camera::dir_right() const {
    return local_to_world_direction(float3(1, 0, 0));
}
float3 Camera::dir_up() const {
    return local_to_world_direction(float3(0, 1, 0));
}

std::array<float3, 4> Camera::frustum_plane_points(float z_depth, float2 min_projection, float2 max_projection) const {
    std::array<float3, 4> corners;
    float upLength = z_depth * tan(this->fov * 0.5f);
    float rightLength = upLength * aspect_ratio;
    float3 farPoint = make_float3(position) + z_depth * dir_forward();
    float3 upVec = upLength * dir_up();
    float3 rightVec = -rightLength * dir_right();
    corners[0] = farPoint + upVec * min_projection.y + rightVec * min_projection.x;
    corners[1] = farPoint + upVec * min_projection.y + rightVec * max_projection.x;
    corners[2] = farPoint + upVec * max_projection.y + rightVec * min_projection.x;
    corners[3] = farPoint + upVec * max_projection.y + rightVec * max_projection.x;
    return corners;
}

std::array<float3, 8> Camera::frustum_corners(float2 min_projection, float2 max_projection) const {
    std::array<float3, 8> corners;
    float fov = tan(this->fov * 0.5f);
    float3 forward = dir_forward();
    float3 up = dir_up();
    float3 right = dir_right();
    auto get_corner = [&](float3 *corners, float dist, float3 position, float aspect) {
        float upLength = dist * (fov);
        float rightLength = upLength * aspect;
        float3 farPoint = position + dist * forward;
        float3 upVec = upLength * up;
        float3 rightVec = -rightLength * right;
        corners[0] = farPoint + upVec * min_projection.y + rightVec * min_projection.x;
        corners[1] = farPoint + upVec * min_projection.y + rightVec * max_projection.x;
        corners[2] = farPoint + upVec * max_projection.y + rightVec * min_projection.x;
        corners[3] = farPoint + upVec * max_projection.y + rightVec * max_projection.x;
    };

    get_corner(
        corners.data(),
        near_plane,
        make_float3(position),
        aspect_ratio);
    get_corner(
        corners.data() + 4,
        far_plane,
        make_float3(position),
        aspect_ratio);
    return corners;
}

std::array<float4, 6> Camera::frustum_plane(float2 min_projection, float2 max_projection) const {
    std::array<float4, 6> planes;
    auto corners = frustum_plane_points(far_plane, min_projection, max_projection);
    auto position = make_float3(this->position);
    float3 forward = dir_forward();
    planes[0] = get_plane(corners[1], corners[0], position);
    planes[1] = get_plane(corners[2], corners[3], position);
    planes[2] = get_plane(corners[0], corners[2], position);
    planes[3] = get_plane(corners[3], corners[1], position);
    planes[4] = get_plane(forward, position + forward * far_plane);
    planes[5] = get_plane(-forward, position + forward * near_plane);
    return planes;
}

RBC_RUNTIME_API bool frustum_cull(
    luisa::float4x4 const &local_to_world,
    luisa::compute::AABB const &bounding,
    luisa::span<luisa::float4, 6> frustum_planes,
    luisa::float3 frustum_min_point,
    luisa::float3 frustum_max_point,
    luisa::float3 cam_forward,
    luisa::float3 cam_pos) {
    float3 global_min{1e20f};
    float3 global_max{-1e20f};
    float3 local_min{bounding.packed_min[0], bounding.packed_min[1], bounding.packed_min[2]};
    float3 local_max{bounding.packed_max[0], bounding.packed_max[1], bounding.packed_max[2]};
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
            for (int z = 0; z < 2; z++) {
                float3 point = select(local_min, local_max, int3(x, y, z) == 1);
                point = (local_to_world * make_float4(point, 1.f)).xyz();
                global_min = min(global_min, point);
                global_max = max(global_max, point);
            }
    float3 position = lerp(global_min, global_max, 0.5f);
    float3 extent = global_max - position;
    if (any(global_min > frustum_max_point) || any(global_max < frustum_min_point)) {
        return false;
    }
    for (auto &plane : frustum_planes) {
        float3 abs_normal = abs(plane.xyz());
        if ((dot(position, plane.xyz()) - dot(abs_normal, extent)) > -plane.w) {
            return false;
        }
    }
    return true;
}

}// namespace rbc