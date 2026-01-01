/**
 * @file scene.cpp
 * @brief The Scene Impl
 * @author sailing-innocent
 * @date 2024-05-17
 */

#include "rbc_particle/scene/scene.h"
#include <iostream>

namespace rbc::ps {

Scene::Scene()
    : m_root(make_float4x4(1.0f)) {
}
Scene::Scene(Scene &&other) {
    m_root = std::move(other.m_root);
}

int Scene::emplace_emitter_node(EmitterRenderStateProxy const &render_state_proxy) noexcept {
    // [TODO]: append a node to the root node
    // [TODO]: create a new EmitterSceneNode
    m_emitter_nodes.emplace_back(std::move(EmitterSceneNode{render_state_proxy, eastl::make_shared<SceneNode3D>()}));
    return m_emitter_nodes.size() - 1;
};

}// namespace rbc::ps