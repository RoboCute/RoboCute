#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/mat_code.h>
#include <rbc_world/mesh_loader.h>
#include <rbc_world/gpu_resource.h>
#include <luisa/vstl/common.h>
#include "RBCEditor/runtime/SceneSync.h"

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
    void updateFromSync(const SceneSync &sync);

    // Get lights for rendering
    Lights &lights() { return lights_; }

    // Check if TLAS is ready for rendering
    bool isReady() const { return tlas_ready_; }

    // Animation transform override
    void setAnimationTransform(int entity_id, const Transform &transform);
    void clearAnimationTransforms();

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
    luisa::unordered_map<int, size_t> entity_map_;             // entity_id -> index in instances_
    luisa::unordered_map<int, Transform> animation_transforms_;// entity_id -> animation transform
    uint32_t light_id_ = 0;
    bool light_initialized_ = false;// Track if light has been initialized
    bool tlas_ready_ = false;

    // Mesh loading and conversion
    RC<DeviceMesh> loadMeshFromFile(const luisa::string &path);

    void convertMeshToBuilder(const luisa::shared_ptr<rbc::Mesh> &mesh,
                              class MeshBuilder &builder);

    // Scene initialization
    void initMaterial();
    void initLight();
    void ensureLightInitialized();// Lazy initialization of light

    // Entity management
    void addEntity(int entity_id, const luisa::string &mesh_path, const Transform &transform);
    void updateEntityTransform(int entity_id, const Transform &transform);
    void removeEntity(int entity_id);
};

}// namespace rbc