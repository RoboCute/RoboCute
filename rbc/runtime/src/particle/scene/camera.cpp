/**
 * @file camera.cpp
 * @brief The Camera for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_particle/scene/camera.h"

namespace rbc::ps {

Camera::Camera() noexcept {}
Camera::Camera(Camera &&) noexcept {}

float4x4 Camera::view2world() noexcept {
    return m_node->get_world_transform();
}

float4x4 Camera::world2view() noexcept {
    if (m_is_view_updated) {
        return m_info.view_matrix;
    }
    m_info.view_matrix = inverse(view2world());
}

float4x4 Camera::projection() noexcept {
    if (m_is_proj_updated) {
        return m_info.projection_matrix;
    }
    _update_projection();
}

void Camera::_update_projection() noexcept {}

}// namespace rbc::ps