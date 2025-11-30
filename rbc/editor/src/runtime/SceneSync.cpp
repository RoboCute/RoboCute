

#include "RBCEditor/runtime/SceneSync.h"
#include <luisa/core/logging.h>
#include <rbc_core/json_serde.h>

namespace rbc {

SceneSync::SceneSync()
    : has_changes_(false) {
}

SceneSync::~SceneSync() = default;

const SceneEntity *SceneSync::getEntity(int entity_id) const {
    auto it = entity_map_.find(entity_id);
    if (it != entity_map_.end()) {
        return &entities_[it->second];
    }
    return nullptr;
}

const EditorResourceMetadata *SceneSync::getResource(int resource_id) const {
    auto it = resource_map_.find(resource_id);
    if (it != resource_map_.end()) {
        return &resources_[it->second];
    }
    return nullptr;
}

/**
* ParseSceneState

*/

bool SceneSync::parseSceneState(const std::string &json) {
    // TODO: 未来改为增量场景传输
    try {
        JsonReader reader(json);

        // Read root "scene" object
        if (!reader.start_object("scene")) {
            LUISA_WARNING("No 'scene' object in JSON");
            // TODO: Error Handling for Invalid Response
            return false;
        }

        // Read "entities" array
        uint64_t entity_count = 0;
        if (!reader.start_array(entity_count, "entities")) {
            LUISA_INFO("No entities in scene");
            entities_.clear();
            entity_map_.clear();
            reader.end_scope();// end scene object
            return true;
        }

        // Parse entities
        luisa::vector<SceneEntity> new_entities;
        for (uint64_t i = 0; i < entity_count; ++i) {
            if (reader.start_object()) {
                SceneEntity entity = parseEntity(reader);
                new_entities.emplace_back(entity);
                reader.end_scope();// end entity object
            }
        }

        reader.end_scope();// end entities array
        reader.end_scope();// end scene object

        // Check if entities changed
        bool changed = (new_entities.size() != entities_.size());
        if (!changed) {
            for (size_t i = 0; i < new_entities.size(); ++i) {
                if (new_entities[i].id != entities_[i].id) {
                    changed = true;
                    break;
                }
            }
        }
        // 当前为了方便开发我们每次全量更新
        // Update entities
        entities_ = new_entities;
        entity_map_.clear();
        for (size_t i = 0; i < entities_.size(); ++i) {
            entity_map_[entities_[i].id] = i;
        }

        has_changes_ = changed;
        return changed;

    } catch (const std::exception &e) {
        LUISA_ERROR("Failed to parse scene state: {}", e.what());
        return false;
    }
}

SceneEntity SceneSync::parseEntity(JsonReader &reader) {
    SceneEntity entity;

    // Read id and name
    int64_t id_val = 0;
    reader.read(id_val, "id");
    entity.id = static_cast<int>(id_val);

    reader.read(entity.name, "name");

    // Read components object
    if (reader.start_object("components")) {
        // Parse transform
        if (reader.start_object("transform")) {
            entity.transform = parseTransform(reader);
            reader.end_scope();// end transform
        }

        // Parse render component
        if (reader.start_object("render")) {
            entity.render_component = parseRenderComponent(reader);
            entity.has_render_component = true;
            reader.end_scope();// end render
        }

        reader.end_scope();// end components
    }

    return entity;
}

Transform SceneSync::parseTransform(JsonReader &reader) {
    Transform transform;

    // Read position array
    uint64_t pos_size = 0;
    if (reader.start_array(pos_size, "position") && pos_size >= 3) {
        double x, y, z;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        transform.position = luisa::float3(static_cast<float>(x),
                                           static_cast<float>(y),
                                           static_cast<float>(z));
        reader.end_scope();// end position array
    }

    // Read rotation array (quaternion)
    uint64_t rot_size = 0;
    if (reader.start_array(rot_size, "rotation") && rot_size >= 4) {
        double x, y, z, w;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        reader.read(w);
        transform.rotation = luisa::float4(static_cast<float>(x),
                                           static_cast<float>(y),
                                           static_cast<float>(z),
                                           static_cast<float>(w));
        reader.end_scope();// end rotation array
    }

    // Read scale array
    uint64_t scale_size = 0;
    if (reader.start_array(scale_size, "scale") && scale_size >= 3) {
        double x, y, z;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        transform.scale = luisa::float3(static_cast<float>(x),
                                        static_cast<float>(y),
                                        static_cast<float>(z));
        reader.end_scope();// end scale array
    }

    return transform;
}

RenderComponent SceneSync::parseRenderComponent(JsonReader &reader) {
    RenderComponent render;

    int64_t mesh_id_val = 0;
    if (reader.read(mesh_id_val, "mesh_id")) {
        render.mesh_id = static_cast<int>(mesh_id_val);
    }

    // Read material_ids array if present
    uint64_t mat_count = 0;
    if (reader.start_array(mat_count, "material_ids")) {
        for (uint64_t i = 0; i < mat_count; ++i) {
            int64_t mat_id = 0;
            if (reader.read(mat_id)) {
                render.material_ids.push_back(static_cast<int>(mat_id));
            }
        }
        reader.end_scope();// end material_ids array
    }

    return render;
}

bool SceneSync::parseResources(const std::string &json) {
    // parse all resource metadata
    try {
        JsonReader reader(json);

        // Read "resources" array from root
        uint64_t resource_count = 0;
        if (!reader.start_array(resource_count, "resources")) {
            LUISA_INFO("No resources in response");
            return false;
        }

        // Parse resources
        luisa::vector<EditorResourceMetadata> new_resources;
        for (uint64_t i = 0; i < resource_count; ++i) {
            if (reader.start_object()) {
                EditorResourceMetadata resource = parseResourceMetadata(reader);
                new_resources.emplace_back(resource);
                reader.end_scope();// end resource object
            }
        }

        reader.end_scope();// end resources array

        // Check if resources changed
        bool changed = (new_resources.size() != resources_.size());

        // Update resources
        resources_ = new_resources;
        resource_map_.clear();
        for (size_t i = 0; i < resources_.size(); ++i) {
            resource_map_[resources_[i].id] = i;
        }

        return changed;

    } catch (const std::exception &e) {
        LUISA_ERROR("Failed to parse resources: {}", e.what());
        return false;
    }
}

EditorResourceMetadata SceneSync::parseResourceMetadata(JsonReader &reader) {

    EditorResourceMetadata resource;
    int64_t id_val = 0;
    reader.read(id_val, "id");
    resource.id = static_cast<int>(id_val);

    int64_t type_val = 0;
    reader.read(type_val, "type");
    resource.type = static_cast<int>(type_val);

    reader.read(resource.path, "path");

    int64_t state_val = 0;
    reader.read(state_val, "state");
    resource.state = static_cast<int>(state_val);

    int64_t size_val = 0;
    reader.read(size_val, "memory_size");
    resource.size_bytes = static_cast<size_t>(size_val);

    return resource;
}

const AnimationClip *SceneSync::getAnimation(const luisa::string &name) const {
    auto it = animation_map_.find(name);
    if (it != animation_map_.end()) {
        return &animations_[it->second];
    }
    return nullptr;
}

bool SceneSync::parseAnimations(const std::string &json) {
    try {
        JsonReader reader(json);

        // Read "animations" array from root
        uint64_t anim_count = 0;
        if (!reader.start_array(anim_count, "animations")) {
            LUISA_INFO("No animations in response");
            return false;
        }

        // Parse animations
        luisa::vector<AnimationClip> new_animations;
        for (uint64_t i = 0; i < anim_count; ++i) {
            if (reader.start_object()) {
                AnimationClip clip = parseAnimationClip(reader);
                new_animations.emplace_back(clip);
                reader.end_scope();// end animation object
            }
        }

        reader.end_scope();// end animations array

        // Check if animations changed
        bool changed = (new_animations.size() != animations_.size());

        // Update animations
        animations_ = new_animations;
        animation_map_.clear();
        for (size_t i = 0; i < animations_.size(); ++i) {
            animation_map_[animations_[i].name] = i;
        }

        LUISA_INFO("Parsed {} animations", animations_.size());
        return changed;

    } catch (const std::exception &e) {
        LUISA_ERROR("Failed to parse animations: {}", e.what());
        return false;
    }
}

AnimationClip SceneSync::parseAnimationClip(JsonReader &reader) {
    AnimationClip clip;

    reader.read(clip.name, "name");
    
    double fps_val = 30.0;
    reader.read(fps_val, "fps");
    clip.fps = static_cast<float>(fps_val);

    int64_t total_frames_val = 0;
    reader.read(total_frames_val, "total_frames");
    clip.total_frames = static_cast<int>(total_frames_val);

    double duration_val = 0.0;
    reader.read(duration_val, "duration_seconds");
    clip.duration_seconds = static_cast<float>(duration_val);

    // Read sequences array
    uint64_t seq_count = 0;
    if (reader.start_array(seq_count, "sequences")) {
        for (uint64_t i = 0; i < seq_count; ++i) {
            if (reader.start_object()) {
                AnimationSequence seq = parseAnimationSequence(reader);
                clip.sequences.emplace_back(seq);
                reader.end_scope();// end sequence object
            }
        }
        reader.end_scope();// end sequences array
    }

    return clip;
}

AnimationSequence SceneSync::parseAnimationSequence(JsonReader &reader) {
    AnimationSequence seq;

    int64_t entity_id_val = 0;
    reader.read(entity_id_val, "entity_id");
    seq.entity_id = static_cast<int>(entity_id_val);

    // Read keyframes array
    uint64_t keyframe_count = 0;
    if (reader.start_array(keyframe_count, "keyframes")) {
        for (uint64_t i = 0; i < keyframe_count; ++i) {
            if (reader.start_object()) {
                AnimationKeyframe kf = parseAnimationKeyframe(reader);
                seq.keyframes.emplace_back(kf);
                reader.end_scope();// end keyframe object
            }
        }
        reader.end_scope();// end keyframes array
    }

    return seq;
}

AnimationKeyframe SceneSync::parseAnimationKeyframe(JsonReader &reader) {
    AnimationKeyframe kf;

    int64_t frame_val = 0;
    reader.read(frame_val, "frame");
    kf.frame = static_cast<int>(frame_val);

    // Read position array
    uint64_t pos_size = 0;
    if (reader.start_array(pos_size, "position") && pos_size >= 3) {
        double x, y, z;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        kf.position[0] = static_cast<float>(x);
        kf.position[1] = static_cast<float>(y);
        kf.position[2] = static_cast<float>(z);
        reader.end_scope();// end position array
    }

    // Read rotation array (quaternion)
    uint64_t rot_size = 0;
    if (reader.start_array(rot_size, "rotation") && rot_size >= 4) {
        double x, y, z, w;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        reader.read(w);
        kf.rotation[0] = static_cast<float>(x);
        kf.rotation[1] = static_cast<float>(y);
        kf.rotation[2] = static_cast<float>(z);
        kf.rotation[3] = static_cast<float>(w);
        reader.end_scope();// end rotation array
    }

    // Read scale array
    uint64_t scale_size = 0;
    if (reader.start_array(scale_size, "scale") && scale_size >= 3) {
        double x, y, z;
        reader.read(x);
        reader.read(y);
        reader.read(z);
        kf.scale[0] = static_cast<float>(x);
        kf.scale[1] = static_cast<float>(y);
        kf.scale[2] = static_cast<float>(z);
        reader.end_scope();// end scale array
    }

    return kf;
}

}// namespace rbc
