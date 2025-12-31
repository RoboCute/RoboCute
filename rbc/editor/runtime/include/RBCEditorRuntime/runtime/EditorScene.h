#pragma once
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/components/render_component.h>
#include <luisa/vstl/common.h>

#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {

/**
 * Editor scene that synchronizes with Python server
 * 
 * Uses Runtime's Entity-Component-Resource system for scene management.
 * Manages entities with TransformComponent and RenderComponent based on
 * scene state from the server.
 */
class EditorScene {
public:
    EditorScene();
    ~EditorScene();

    // Update scene from synchronized state
    void updateFromSync(const SceneSync &sync);

    // Called each frame to process pending render operations
    // Should be called after IO operations are complete (e.g., after execute_io in tick)
    void onFrameTick();

    // Check if scene is ready for rendering
    [[nodiscard]] bool isReady() const { return scene_ready_; }

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
    // Entity tracking info
    struct EntityInfo {
        int entity_id = 0;
        luisa::string mesh_path;
        world::Entity *entity = nullptr;
        RC<world::MeshResource> mesh_resource;
    };

    // Entities managed by this scene
    luisa::vector<EntityInfo> entities_;
    luisa::unordered_map<int, size_t> entity_map_;// entity_id -> index in entities_

    // Mesh resource cache (keyed by path)
    luisa::unordered_map<luisa::string, RC<world::MeshResource>> mesh_cache_;

    // Default material for all entities
    RC<world::MaterialResource> default_material_;

    // Default skybox texture (loaded from sky.exr)
    RC<world::TextureResource> default_skybox_;

    // Animation transform overrides
    luisa::unordered_map<int, Transform> animation_transforms_;// entity_id -> animation transform

    // Pending entities that need start_update_object called after IO completes
    struct PendingEntity {
        int entity_id;
        world::Entity *entity;
        RC<world::MeshResource> mesh_resource;
    };
    luisa::vector<PendingEntity> pending_entities_;

    // State flags
    bool world_initialized_ = false;
    bool scene_ready_ = false;

    // Scene initialization
    void initWorld();
    void initMaterial();
    void initSkybox();

    // Entity management
    void addEntity(int entity_id, const luisa::string &mesh_path, const Transform &transform);
    void updateEntityTransform(int entity_id, const Transform &transform);
    void removeEntity(int entity_id);

    // Mesh loading
    RC<world::MeshResource> loadMeshResource(const luisa::string &path);

    // Process pending entities (called after IO completes)
    void processPendingEntities();
};

}// namespace rbc
