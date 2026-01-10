#pragma once
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_graphics/camera.h>
#include <rbc_core/coroutine.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>

namespace rbc {
struct ClickManager;
struct GraphicsUtils;
struct MeshBuilder;
struct WorldScene {
    luisa::filesystem::path scene_root_dir;
    luisa::filesystem::path entities_path;
    world::MeshResource *cbox_mesh{};
    world::MeshResource *quad_mesh{};
    RC<world::TextureResource> skybox{};
    RC<world::TextureResource> tex{};
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::MaterialResource>> _mats;
    // skinning
    Buffer<DualQuaternion> test_bones;
    RC<world::MeshResource> skinning_origin_mesh;
    RC<world::MeshResource> skinning_mesh;
    RC<world::Entity> skinning_entity;// make it independent, no save to file
    // jolt physics
    RC<world::MaterialResource> physics_mat;
    RC<world::Entity> physics_floor_entity;// make it independent, no save to file
    RC<world::Entity> physics_box_entity;  // make it independent, no save to file
    RC<world::MeshResource> physics_box_mesh;

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
        ClickManager& click_mng,
        Camera const &cam);
    void tick_skinning(GraphicsUtils *utils, float delta_time);
private:
    RC<Gizmos> _gizmos;
    void _init_scene(GraphicsUtils *utils);
    void _init_skinning(GraphicsUtils *utils);
    void _init_physics(GraphicsUtils *utils);
    void _set_gizmos();
    void _write_scene();
    void _create_cube(MeshBuilder &mesh_builder, float3 offset, float3 scale);
};
}// namespace rbc