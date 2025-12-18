#pragma once
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/mesh.h>
#include <rbc_world_v2/texture.h>
#include <rbc_world_v2/material.h>
namespace rbc {
struct GraphicsUtils;
struct WorldScene {
    world::Mesh* cbox_mesh{};
    world::Mesh* quad_mesh{};
    RC<world::Texture> tex{};
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::Material>> _mats;
    WorldScene(GraphicsUtils *utils);
    ~WorldScene();
private:
    void _init_scene(GraphicsUtils *utils);
};
}// namespace rbc