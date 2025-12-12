#pragma once
#include <string>
#include <unordered_map>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/string.h>
#include <rbc_core/json_serde.h>
#include "RBCEditorRuntime/animation/AnimationClip.h"

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

    // Parse scene state from JSON string (typically from HttpClient)
    bool parseSceneState(const std::string &json);
    bool parseResources(const std::string &json);
    bool parseAnimations(const std::string &json);

    // Get entities
    [[nodiscard]] const luisa::vector<SceneEntity> &entities() const { return entities_; }
    [[nodiscard]] const SceneEntity *getEntity(int entity_id) const;

    // Get resources
    [[nodiscard]] const luisa::vector<EditorResourceMetadata> &resources() const { return resources_; }
    [[nodiscard]] const EditorResourceMetadata *getResource(int resource_id) const;

    // Get animations
    [[nodiscard]] const luisa::vector<AnimationClip> &animations() const { return animations_; }
    [[nodiscard]] const AnimationClip *getAnimation(const luisa::string &name) const;

    // Check if scene has changed since last update
    [[nodiscard]] bool hasChanges() const { return has_changes_; }
    void clearChanges() { has_changes_ = false; }

private:
    luisa::vector<SceneEntity> entities_;
    luisa::vector<EditorResourceMetadata> resources_;
    luisa::vector<AnimationClip> animations_;
    luisa::unordered_map<int, size_t> entity_map_;             // entity_id -> index in entities_
    luisa::unordered_map<int, size_t> resource_map_;           // resource_id -> index in resources_
    luisa::unordered_map<luisa::string, size_t> animation_map_;// animation name -> index in animations_
    bool has_changes_;

    // JSON parsing helpers using yyjson
    SceneEntity parseEntity(JsonReader &reader);
    Transform parseTransform(JsonReader &reader);
    RenderComponent parseRenderComponent(JsonReader &reader);
    EditorResourceMetadata parseResourceMetadata(JsonReader &reader);
    AnimationClip parseAnimationClip(JsonReader &reader);
    AnimationSequence parseAnimationSequence(JsonReader &reader);
    AnimationKeyframe parseAnimationKeyframe(JsonReader &reader);
};

}// namespace rbc
