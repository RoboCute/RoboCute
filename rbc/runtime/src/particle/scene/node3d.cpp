/**
 * @file node3d.cpp
 * @brief The Implementation of SceneNode3D
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_particle/scene/node3d.h"

namespace rbc::ps {

SceneNode3D::SceneNode3D(float4x4 const &transform)
    : m_local_transform(transform) {
}
SceneNode3D::SceneNode3D(SceneNode3D &&) {
    // move the local transform
    m_local_transform = std::move(m_local_transform);
    make_dirty();
}
SceneNode3D::~SceneNode3D() {}

void SceneNode3D::set_local_transform(float4x4 const &transform) noexcept {
    m_local_transform = transform;
    make_dirty();
}
float4x4 const &SceneNode3D::get_local_transform() const noexcept {
    return m_local_transform;
}
float4x4 const &SceneNode3D::get_world_transform() noexcept {
    if (!m_is_updated) { update(); }
    return m_world_transform;
}

void SceneNode3D::update() noexcept {
    // update world_transform according to parent world transform and local transform
    m_world_transform = m_local_transform * (m_parent ? m_parent->get_world_transform() : make_float4x4(1.0f));

    m_is_updated = true;
    // mark dirty for all children
    for (auto &child : m_children) {
        child->make_dirty();
    }
}

}// namespace rbc::ps