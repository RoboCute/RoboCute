#pragma once
#include <string>
#include <unordered_map>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/string.h>

namespace rbc {

/**
 * Transform data from Python scene
 */
struct Transform {
    luisa::float3 position{0.0f, 0.0f, 0.0f};
    luisa::float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};// Quaternion (x, y, z, w)
    luisa::float3 scale{1.0f, 1.0f, 1.0f};
};

/**
 * Render component data from Python scene
 */
struct RenderComponent {
    int mesh_id = 0;
    luisa::vector<int> material_ids;
};

/**
 * Entity in the synchronized scene
 */
struct SceneEntity {
    int id = 0;
    luisa::string name;
    Transform transform;
    RenderComponent render_component;
    bool has_render_component = false;
};

/**
 * Resource metadata from Python
 */
struct EditorResourceMetadata {
    int id = 0;
    int type = 0;// ResourceType enum
    luisa::string path;
    int state = 0;// ResourceState enum
    size_t size_bytes = 0;
};

/**
 * Scene synchronization manager
 * 
 * Manages local mirror of Python scene state by:
 * - Fetching scene state from HTTP server
 * - Parsing JSON data
 * - Tracking entity and resource changes
 */
class SceneSync {
public:
    SceneSync();
    ~SceneSync();

    // Fetch and update scene state from server
    // Returns true if scene was updated
    bool update_from_server(class HttpClient &client);

    // Get entities
    const luisa::vector<SceneEntity> &entities() const { return entities_; }
    const SceneEntity *get_entity(int entity_id) const;

    // Get resources
    const luisa::vector<EditorResourceMetadata> &resources() const { return resources_; }
    const EditorResourceMetadata *get_resource(int resource_id) const;

    // Check if scene has changed since last update
    bool has_changes() const { return has_changes_; }
    void clear_changes() { has_changes_ = false; }

private:
    luisa::vector<SceneEntity> entities_;
    luisa::vector<EditorResourceMetadata> resources_;
    luisa::unordered_map<int, size_t> entity_map_;  // entity_id -> index in entities_
    luisa::unordered_map<int, size_t> resource_map_;// resource_id -> index in resources_
    bool has_changes_;

    // JSON parsing helpers
    bool parse_scene_state(const luisa::string &json);
    bool parse_resources(const luisa::string &json);
    SceneEntity parse_entity(const luisa::string &json_object);
    Transform parse_transform(const luisa::string &json_object);
    RenderComponent parse_render_component(const luisa::string &json_object);
    EditorResourceMetadata parse_resource(const luisa::string &json_object);

    // Simple JSON parsing utilities (no external dependencies)
    luisa::string json_get_string(const luisa::string &json, const char *key);
    int json_get_int(const luisa::string &json, const char *key);
    float json_get_float(const luisa::string &json, const char *key);
    luisa::string json_get_object(const luisa::string &json, const char *key);
    luisa::vector<luisa::string> json_get_array_objects(const luisa::string &json, const char *key);
    luisa::vector<float> json_get_float_array(const luisa::string &json, const char *key);
};

}// namespace rbc
