#pragma once

#include <rbc_config.h>
#include <rbc_world/entity.h>
#include <rbc_world/base_object.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc {

class GraphicsUtils;

/**
 * Scene source type - determines data origin
 */
enum class EditorSceneSourceType {
    None,  // No source configured
    Server,// Connected to Python server via HTTP
    Local, // Loading from local .rbcscene file / project directory
};

// Forward declarations
struct SceneSyncData;
class EditorScene;

/**
 * Transform data structure for whole scene (shared between server and local modes)
 */
struct SceneTransform {
    luisa::float3 position{0.0f, 0.0f, 0.0f};
    luisa::float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};// Quaternion (x, y, z, w)
    luisa::float3 scale{1.0f, 1.0f, 1.0f};
};

/**
 * Entity descriptor for scene synchronization (SERVER mode)
 * Represents entity data received from Python server
 */
struct ServerEntityData {
    int entity_id = 0;
    luisa::string name;
    luisa::string mesh_path;
    SceneTransform transform;
    bool has_render_component = false;
};

/**
 * SceneSyncData - Parsed scene data from server
 * Used by EditorScene in SERVER mode to update entities
 */
struct RBC_EDITOR_RUNTIME_API SceneSyncData {
    luisa::vector<ServerEntityData> entities;
    luisa::unordered_map<int, size_t> entity_map;// entity_id -> index
    bool has_changes = false;

    void clear();
    bool parseFromJson(const luisa::string &json);
    const ServerEntityData *getEntity(int entity_id) const;
};

/**
 * EditorScene - Runtime scene for the editor
 * 
 * Manages entities and resources for rendering and editing.
 * Supports two data sources:
 * 
 * 1. SERVER mode: Scene data synchronized from Python server
 *    - Entities created/updated based on SceneSyncData
 *    - Mesh paths from server are loaded as MeshResource
 *    - Suitable for remote editing / collaborative workflows
 * 
 * 2. LOCAL mode: Scene loaded from local project directory
 *    - Uses world::init_world() to initialize project context
 *    - Entities deserialized from .rbcscene files
 *    - Full access to project resources (textures, materials, etc.)
 *    - Suitable for standalone editor / offline workflows
 * 
 * The scene provides a unified interface regardless of data source,
 * allowing the renderer and UI to work without knowing the source type.
 */
class RBC_EDITOR_RUNTIME_API EditorScene {
public:
    EditorScene();
    ~EditorScene();

    // ========== Initialization ==========

    /**
     * Initialize scene in SERVER mode
     * @param serverUrl The Python server URL for scene sync
     * @return true if initialization successful
     */
    bool initFromServer(const luisa::string &serverUrl);

    /**
     * Initialize scene in LOCAL mode from project directory
     * @param projectPath Path to the project folder (containing .rbc files)
     * @return true if initialization successful
     */
    bool initFromLocal(const luisa::filesystem::path &projectPath);

    /**
     * Shutdown scene and release all resources
     */
    void shutdown();

    // ========== Scene State ==========

    /**
     * Get current source type
     */
    [[nodiscard]] EditorSceneSourceType sourceType() const { return source_type_; }

    /**
     * Check if scene is ready for rendering
     */
    [[nodiscard]] bool isReady() const { return scene_ready_; }

    /**
     * Check if world is initialized
     */
    [[nodiscard]] bool isWorldInitialized() const { return world_initialized_; }

    // ========== Frame Update ==========

    /**
     * Update scene state each frame
     * - SERVER mode: Apply pending sync data
     * - LOCAL mode: Process async resource loading
     * Should be called after IO operations complete
     */
    void onFrameTick();

    /**
     * Update scene from server sync data (SERVER mode)
     */
    void updateFromSync(const SceneSyncData &syncData);

    // ========== Entity Access ==========

    /**
     * Get entity by local ID
     * @return Entity pointer or nullptr if not found
     */
    [[nodiscard]] world::Entity *getEntity(int localId) const;

    /**
     * Get all entity local IDs
     */
    [[nodiscard]] luisa::vector<int> getAllEntityIds() const;

    /**
     * Get entity count
     */
    [[nodiscard]] size_t entityCount() const { return entities_.size(); }

    /**
     * Get entity local ID from TLAS instance ID (for picking)
     * @return local ID or -1 if not found
     */
    [[nodiscard]] int getEntityIdFromInstanceId(uint32_t instanceId) const;

    /**
     * Get entity transform
     */
    [[nodiscard]] SceneTransform getEntityTransform(int localId) const;

    /**
     * Set entity transform
     */
    void setEntityTransform(int localId, const SceneTransform &transform);

    // ========== Resource Access ==========

    /**
     * Get the skybox texture resource
     */
    [[nodiscard]] RC<world::TextureResource> getSkybox() const { return skybox_; }

    /**
     * Set graphics utils reference for resource updates
     */
    void setGraphicsUtils(GraphicsUtils *utils) { graphics_utils_ = utils; }

    // ========== Scene Modification (LOCAL mode) ==========

    /**
     * Create a new entity in the scene (LOCAL mode only)
     * @return Local ID of the new entity, or -1 on failure
     */
    int createEntity(const luisa::string &name);

    /**
     * Delete an entity from the scene (LOCAL mode only)
     * @return true if entity was deleted
     */
    bool deleteEntity(int localId);

    /**
     * Save scene to disk (LOCAL mode only)
     * @return true if save successful
     */
    bool saveScene();

private:
    // ========== Internal Entity Tracking ==========

    struct EntityEntry {
        int local_id = 0;
        luisa::string name;
        RC<world::Entity> entity;
        RC<world::MeshResource> mesh_resource;// cached mesh
        luisa::string mesh_path;              // for SERVER mode: original path
        uint32_t instance_id = 0;             // TLAS instance ID for picking
    };

    // Entity storage
    luisa::vector<EntityEntry> entities_;
    luisa::unordered_map<int, size_t> entity_map_;          // local_id -> index
    luisa::unordered_map<uint32_t, int> instance_to_entity_;// instance_id -> local_id
    int next_local_id_ = 1;

    // Resource caches
    luisa::unordered_map<luisa::string, RC<world::MeshResource>> mesh_cache_;
    luisa::vector<RC<world::MaterialResource>> materials_;
    RC<world::MaterialResource> default_material_;
    RC<world::TextureResource> skybox_;

    // Pending async operations
    struct PendingEntity {
        int local_id;
        RC<world::Entity> entity;
        RC<world::MeshResource> mesh_resource;
    };
    luisa::vector<PendingEntity> pending_entities_;

    // Scene state
    EditorSceneSourceType source_type_ = EditorSceneSourceType::None;
    luisa::filesystem::path project_path_;
    luisa::filesystem::path scene_file_path_;
    luisa::string server_url_;
    bool world_initialized_ = false;
    bool scene_ready_ = false;
    GraphicsUtils *graphics_utils_ = nullptr;

    // ========== Internal Methods ==========

    // Initialization helpers
    void initWorld(const luisa::filesystem::path &metaPath);
    void initDefaultMaterial();
    void initSkybox(const luisa::filesystem::path &skyboxPath = {});

    // Entity management (SERVER mode)
    void addEntityFromServer(const ServerEntityData &data);
    void updateEntityFromServer(int localId, const ServerEntityData &data);
    void removeEntityByLocalId(int localId);

    // Entity management (LOCAL mode)
    void loadEntitiesFromScene();
    void writeEntitiesToScene();

    // Resource loading
    RC<world::MeshResource> loadMeshResource(const luisa::string &path);

    // Process pending async loads
    void processPendingEntities();

    // Instance ID management
    void updateInstanceMapping(int localId, uint32_t instanceId);
};

}// namespace rbc