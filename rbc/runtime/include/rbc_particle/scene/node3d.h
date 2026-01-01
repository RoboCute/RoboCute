#pragma once
/**
 * @file node3d.h
 * @brief The SceneNode, each defines a translation [in x,y,z] and a rotation [in x,y,z,w]
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_config.h"
#include <luisa/luisa-compute.h>

namespace rbc::ps {

class SceneNode3D {
    luisa::float4x4 m_local_transform;
    luisa::float4x4 m_world_transform;
    bool m_is_updated = false;
    bool m_is_root = false;
    bool m_is_visible = false;

    eastl::shared_ptr<SceneNode3D> m_parent;
    eastl::vector<eastl::shared_ptr<SceneNode3D>> m_children;

public:
    SceneNode3D(luisa::float4x4 const &transform = luisa::make_float4x4(1.0f));
    virtual ~SceneNode3D();
    // delete copy, enable move
    SceneNode3D(SceneNode3D const &) = delete;
    SceneNode3D &operator=(SceneNode3D const &) = delete;
    SceneNode3D(SceneNode3D &&);
    SceneNode3D &operator=(SceneNode3D &&) = default;

    // getter
    [[nodiscard]] luisa::float4x4 const &get_local_transform() const noexcept;
    [[nodiscard]] luisa::float4x4 const &get_world_transform() noexcept;

    [[nodiscard]] bool is_updated() const { return m_is_updated; }
    [[nodiscard]] bool is_visible() const { return m_is_visible; }
    [[nodiscard]] bool is_root() const { return m_is_root; }

    // setter
    void set_local_transform(luisa::float4x4 const &transform) noexcept;
    void make_dirty() noexcept { m_is_updated = false; }
    void make_root() noexcept { m_is_root = true; }
    void make_visible() noexcept { m_is_visible = true; }
    void make_invisible() noexcept { m_is_visible = false; }
    // update world transform according to parent world transform and local transform
    void update() noexcept;
};

}// namespace rbc::ps