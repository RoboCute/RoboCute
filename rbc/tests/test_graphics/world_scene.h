#pragma once
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/mesh.h>
#include <rbc_world_v2/texture.h>
#include <rbc_world_v2/material.h>
#include <rbc_graphics/camera.h>
namespace rbc {
struct GraphicsUtils;
struct WorldScene {

    world::Mesh *cbox_mesh{};
    world::Mesh *quad_mesh{};
    RC<world::Texture> tex{};
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::Material>> _mats;
    struct Gizmos : RCBase {
        uint vertex_size;
        Buffer<uint> data;
        float2 picked_uv;
        bool picking{};
        float to_cam_distance;
        float3 relative_pos;
    };
    WorldScene(GraphicsUtils *utils);
    ~WorldScene();
    bool draw_gizmos(
        bool dragging,
        GraphicsUtils *utils,
        uint2 click_pixel_pos,
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