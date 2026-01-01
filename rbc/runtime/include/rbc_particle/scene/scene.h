#pragma once
/**
 * @file scene.h
 * @brief The Scene for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_config.h"
#include "rbc_particle/scene/node3d.h"
#include "rbc_particle/scene/camera.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

namespace rbc::ps {

struct EmitterRenderStateProxy;

struct EmitterSceneNode {
    EmitterRenderStateProxy const &m_render_state_proxy;
    eastl::shared_ptr<SceneNode3D> mp_node;
};

class Scene {
    friend class ParticleSystem;
    // root node
    SceneNode3D m_root;// geometry key entry
    eastl::unique_ptr<Camera> mp_active_camera;

    eastl::vector<EmitterSceneNode> m_emitter_nodes;

public:
    // delete copy constructor
    Scene();
    Scene(Scene const &) = delete;
    Scene &operator=(Scene const &) = delete;
    // enable move
    Scene(Scene &&);
    Scene &operator=(Scene &&) = default;

    // getter
    [[nodiscard]] EmitterSceneNode const &get_emit_node(int i) noexcept {
        return m_emitter_nodes[i];
    }

private:
    // return the emplaced index
    int emplace_emitter_node(EmitterRenderStateProxy const &render_state_proxy) noexcept;
};

}// namespace rbc::ps