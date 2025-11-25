#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/mat_code.h>
#include <rbc_world/mesh_loader.h>
#include <rbc_world/gpu_resource.h>
#include <luisa/vstl/common.h>
#include "scene_sync.h"

namespace rbc {

/**
 * Editor scene that synchronizes with Python server
 * 
 * Manages meshes loaded from file paths and entity transforms
 * based on scene state from the server.
 */
class EditorScene {
public:
    EditorScene();
    ~EditorScene();

    // Update scene from synchronized state
    void update_from_sync(const SceneSync &sync);

    // Get lights for rendering
    Lights &lights() { return lights_; }

    // Check if TLAS is ready for rendering
    bool is_ready() const { return tlas_ready_; }

private:
    struct EntityInstance {
        int entity_id = 0;
        luisa::string mesh_path;
        uint32_t tlas_index = 0;
        RC<DeviceMesh> device_mesh;
        bool mesh_loaded = false;
    };

    Lights lights_;
    MatCode default_mat_code_;
    luisa::vector<EntityInstance> instances_;
    luisa::unordered_map<int, size_t> entity_map_;// entity_id -> index in instances_
    uint32_t light_id_ = 0;
    bool tlas_ready_ = false;

    // Mesh loading and conversion
    RC<DeviceMesh> load_mesh_from_file(const luisa::string &path);
    void convert_mesh_to_builder(const luisa::shared_ptr<rbc::Mesh> &mesh,
                                  class MeshBuilder &builder);

    // Scene initialization
    void init_material();
    void init_light();

    // Entity management
    void add_entity(int entity_id, const luisa::string &mesh_path, const Transform &transform);
    void update_entity_transform(int entity_id, const Transform &transform);
    void remove_entity(int entity_id);
};

}// namespace rbc

