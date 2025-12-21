#pragma once
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/resources/mesh.h>
#include <rbc_world_v2/texture.h>
#include <rbc_world_v2/resources/material.h>
#include <rbc_graphics/camera.h>

namespace rbc {
struct GraphicsUtils;
struct WorldScene {
    world::MeshResource *cbox_mesh{};
    world::MeshResource *quad_mesh{};
    RC<world::Texture> skybox{};
    RC<world::Texture> tex{};
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::MaterialResource>> _mats;

    struct Gizmos : RCBase {
        Buffer<uint> data;
        float3 relative_pos;
        float2 picked_uv;
        uint vertex_size;
        bool picking{};
        float to_cam_distance;
        uint picked_face{~0u};
    };
    WorldScene(GraphicsUtils *utils);
    ~WorldScene();
    bool draw_gizmos(
        bool dragging,
        GraphicsUtils *utils,
        uint2 click_pixel_pos,
        uint2 mouse_pos,
        uint2 window_size,
        double3 const &cam_pos,
        float cam_far_plane,
        Camera const &cam);
private:
    RC<Gizmos> _gizmos;
    void _init_scene(GraphicsUtils *utils);
    void _set_gizmos();
};
}// namespace rbc