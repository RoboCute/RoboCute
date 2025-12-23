#pragma once
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_graphics/camera.h>
#include <rbc_core/coroutine.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>

namespace rbc {
struct GraphicsUtils;
struct WorldScene {
    luisa::filesystem::path scene_root_dir;
    luisa::filesystem::path entities_path;
    world::MeshResource *cbox_mesh{};
    world::MeshResource *quad_mesh{};
    RC<world::TextureResource> skybox{};
    RC<world::TextureResource> tex{};
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::MaterialResource>> _mats;
    ConcurrentQueue<std::pair<coro::coroutine, RCWeak<world::Component>>> _render_thread_coroutines;

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
    void tick();
private:
    void _add_task(coro::coroutine const& c, world::Component* comp);
    RC<Gizmos> _gizmos;
    void _init_scene(GraphicsUtils *utils);
    void _set_gizmos();
    void _write_scene();
};
}// namespace rbc