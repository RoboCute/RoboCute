#pragma once
/**
 * @file camera.h
 * @brief The Camera for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_config.h"
#include "rbc_particle/scene/node3d.h"
#include <luisa/luisa-compute.h>
#include <EASTL/shared_ptr.h>

using namespace luisa;
using namespace luisa::compute;

namespace rbc::ps {

struct CameraInfo {
    float m_near = 0.1f;
    float m_far = 100.0f;
    float m_fov_rad = 1.0f;
    uint2 m_res = {1024u, 1024u};
    float m_aspect = 1.0f;
    // cached view and projection matrix
    float4x4 view_matrix;
    float4x4 projection_matrix;
};

}// namespace rbc::ps

// TODO: LuisaStruct

namespace rbc::ps {

class Camera {
    eastl::shared_ptr<SceneNode3D> m_node;

    // changement of node will affect the view_matrix
    bool m_is_view_updated = false;
    // changement of near, far, fov, res, aspect will affect the view_matrix
    bool m_is_proj_updated = false;
    // changement for resolution scaling wont affect the view_matrix and proj_matrix
    bool m_is_updated = false;

public:
    enum class CameraCoordType {
        FlipZ,
        FlipY
    };
    enum class CameraProjectionType {
        Perspective,
        Orthographic
    };

    Camera() noexcept;
    // delete copy, enable move
    Camera(Camera const &) = delete;
    Camera &operator=(Camera const &) = delete;
    Camera(Camera &&) noexcept;
    Camera &operator=(Camera &&) noexcept = default;

    [[nodiscard]] float4x4 world2view() noexcept;
    [[nodiscard]] float4x4 view2world() noexcept;
    [[nodiscard]] float4x4 projection() noexcept;
    // update view & projection matrix
    void update() noexcept;

private:
    void _update_projection() noexcept;

    CameraInfo m_info;

    CameraCoordType m_coord_type = CameraCoordType::FlipZ;
    CameraProjectionType m_projection_type = CameraProjectionType::Perspective;
};

}// namespace rbc::ps