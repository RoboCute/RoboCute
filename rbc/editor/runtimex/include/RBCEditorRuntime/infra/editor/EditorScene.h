#pragma once
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>

namespace rbc {

class EditorScene;

// The Synchronizing Scene
struct SceneSync {
public:
    SceneSync();
    ~SceneSync();

private:
    EditorScene *scene_;// the bound editor scen
};

// Runtime Scene, provided by SceneService
// used to
// edit
class EditorScene {
public:
    EditorScene();
    ~EditorScene();

private:
    luisa::map<int, RC<world::Entity>> entities_;// local id -> world::Entity
    RC<world::TextureResource> skybox;
    luisa::vector<RC<world::MaterialResource>> _mats;// built-in materials

    bool world_initialized_ = false;
    bool scene_ready_ = false;
};

}// namespace rbc