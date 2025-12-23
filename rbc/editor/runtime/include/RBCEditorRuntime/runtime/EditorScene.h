#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/mat_code.h>
#include <luisa/vstl/common.h>

#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {

struct Vertex {
    float position[3];
    float normal[3];
    float texcoord[2];
    float tangent[4];// xyz = tangent, w = bitangent sign
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // GPU buffers (opaque handles, can be device pointers or buffer IDs)
    void *vertex_buffer = nullptr;
    void *index_buffer = nullptr;

    // Metadata
    std::string name;

    void compute_normals();
};

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

    // Check if TLAS is ready for rendering
    [[nodiscard]] bool isReady() const { return tlas_ready_; }

    // Animation transform override
    void setAnimationTransform(int entity_id, const Transform &transform);
    void clearAnimationTransforms();

    // Instance ID to Entity ID mapping
    // Convert TLAS instance index to entity ID
    // Returns -1 if instance index not found
    [[nodiscard]] int getEntityIdFromInstanceId(uint32_t instance_id) const;

    // Convert multiple instance IDs to entity IDs
    luisa::vector<int> getEntityIdsFromInstanceIds(const luisa::vector<uint> &instance_ids) const;

private:
    struct EntityInstance {
        int entity_id = 0;
        luisa::string mesh_path;
        uint32_t tlas_index = 0;
        RC<DeviceMesh> device_mesh;
        bool mesh_loaded = false;
    };

    vstd::optional<rbc::Lights> lights_;
    MatCode default_mat_code_;
    luisa::vector<EntityInstance> instances_;
    luisa::unordered_map<int, size_t> entity_map_;             // entity_id -> index in instances_
    luisa::unordered_map<int, Transform> animation_transforms_;// entity_id -> animation transform
    uint32_t light_id_ = 0;
    bool light_initialized_ = false;// Track if light has been initialized
    bool tlas_ready_ = false;

    // Mesh loading and conversion
    RC<DeviceMesh> loadMeshFromFile(const luisa::string &path);

    static void convertMeshToBuilder(const luisa::shared_ptr<rbc::Mesh> &mesh,
                                     class MeshBuilder &builder);

    // Scene initialization
    void initMaterial();
    void initLight();
    // Entity management
    void addEntity(int entity_id, const luisa::string &mesh_path, const Transform &transform);
    void updateEntityTransform(int entity_id, const Transform &transform);
    void removeEntity(int entity_id);
};

}// namespace rbc