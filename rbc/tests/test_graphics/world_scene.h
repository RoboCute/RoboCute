#pragma once
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/mesh.h>
#include <rbc_world_v2/material.h>
namespace rbc {
struct GraphicsUtils;
struct WorldScene {
    world::Mesh *mesh;
    world::Material *mat0, *mat1;
    luisa::vector<world::Entity *> _entities;
    luisa::vector<RC<world::Material>> _mats;
    WorldScene(GraphicsUtils *utils);
    ~WorldScene();
};
}// namespace rbc