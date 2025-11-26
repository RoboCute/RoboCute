#include "scene_sync.h"
#include "http_client.h"
#include <luisa/core/logging.h>
#include <rbc_core/json_serde.h>

namespace rbc {

SceneSync::SceneSync()
    : has_changes_(false) {
}

SceneSync::~SceneSync() = default;

bool SceneSync::update_from_server(HttpClient &client) {
    // Fetch scene state
    luisa::string scene_json = client.get_scene_state();
    if (scene_json.empty()) {
        LUISA_WARNING("Failed to fetch scene state from server");
        return false;
    }

    // Fetch resources
    luisa::string resources_json = client.get_all_resources();
    if (resources_json.empty()) {
        LUISA_WARNING("Failed to fetch resources from server");
        return false;
    }

    // Parse scene and resources
    bool scene_updated = parse_scene_state(scene_json);
    bool resources_updated = parse_resources(resources_json);

    has_changes_ = scene_updated || resources_updated;
    return has_changes_;
}

const SceneEntity *SceneSync::get_entity(int entity_id) const {
    auto it = entity_map_.find(entity_id);
    if (it != entity_map_.end()) {
        return &entities_[it->second];
    }
    return nullptr;
}

const EditorResourceMetadata *SceneSync::get_resource(int resource_id) const {
    auto it = resource_map_.find(resource_id);
    if (it != resource_map_.end()) {
        return &resources_[it->second];
    }
    return nullptr;
}

bool SceneSync::parse_scene_state(const luisa::string &json) {
    try {
        JsonReader reader(json);

        // Read root "scene" object
        if (!reader.start_object("scene")) {
            LUISA_WARNING("No 'scene' object in JSON");
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
                SceneEntity entity = parse_entity(reader);
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

        // Update entities
        entities_ = new_entities;
        entity_map_.clear();
        for (size_t i = 0; i < entities_.size(); ++i) {
            entity_map_[entities_[i].id] = i;
        }

        return changed;

    } catch (const std::exception &e) {
        LUISA_ERROR("Failed to parse scene state: {}", e.what());
        return false;
    }
}

SceneEntity SceneSync::parse_entity(JsonReader &reader) {
    SceneEntity entity;

    // Read id and name
    int64_t id_val = 0;
    reader.read(id_val, "id");
    entity.id = static_cast<int>(id_val);

    reader.read(entity.name, "name");

    // LUISA_INFO("    Parsing entity {}: {}", entity.id, entity.name);

    // Read components object
    if (reader.start_object("components")) {
        // Parse transform
        if (reader.start_object("transform")) {
            entity.transform = parse_transform(reader);
            // LUISA_INFO("      Transform: pos=({}, {}, {})",
            //            entity.transform.position.x,
            //            entity.transform.position.y,
            //            entity.transform.position.z);
            reader.end_scope();// end transform
        }

        // Parse render component
        if (reader.start_object("render")) {
            entity.render_component = parse_render_component(reader);
            entity.has_render_component = true;
            // LUISA_INFO("      Render component: mesh_id={}", entity.render_component.mesh_id);
            reader.end_scope();// end render
        } else {
            LUISA_INFO("      No render component");
        }

        reader.end_scope();// end components
    } else {
        LUISA_WARNING("      No components object");
    }

    return entity;
}

Transform SceneSync::parse_transform(JsonReader &reader) {
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

RenderComponent SceneSync::parse_render_component(JsonReader &reader) {
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

bool SceneSync::parse_resources(const luisa::string &json) {
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
                EditorResourceMetadata resource = parse_resource(reader);
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

EditorResourceMetadata SceneSync::parse_resource(JsonReader &reader) {
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

    // LUISA_INFO("      Resource: id={}, type={}, path={}, state={}",
    //            resource.id, resource.type, resource.path, resource.state);

    return resource;
}

}// namespace rbc
