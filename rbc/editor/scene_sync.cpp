#include "scene_sync.h"
#include "http_client.h"
#include <luisa/core/logging.h>
#include <sstream>
#include <algorithm>

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

// Simple JSON parsing helpers (minimal implementation)
luisa::string SceneSync::json_get_string(const luisa::string &json, const char *key) {
    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return "";

    size_t quote_start = json.find('\"', colon_pos);
    if (quote_start == luisa::string::npos) return "";

    size_t quote_end = json.find('\"', quote_start + 1);
    if (quote_end == luisa::string::npos) return "";

    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

int SceneSync::json_get_int(const luisa::string &json, const char *key) {
    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return 0;

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return 0;

    // Skip whitespace
    size_t num_start = colon_pos + 1;
    while (num_start < json.length() && (json[num_start] == ' ' || json[num_start] == '\t' || json[num_start] == '\n'))
        num_start++;

    // Find end of number
    size_t num_end = num_start;
    while (num_end < json.length() && (std::isdigit(json[num_end]) || json[num_end] == '-' || json[num_end] == '.'))
        num_end++;

    if (num_start == num_end) return 0;

    luisa::string num_str = json.substr(num_start, num_end - num_start);
    return std::atoi(num_str.c_str());
}

float SceneSync::json_get_float(const luisa::string &json, const char *key) {
    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return 0.0f;

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return 0.0f;

    size_t num_start = colon_pos + 1;
    while (num_start < json.length() && (json[num_start] == ' ' || json[num_start] == '\t'))
        num_start++;

    size_t num_end = num_start;
    while (num_end < json.length() && (std::isdigit(json[num_end]) || json[num_end] == '-' || json[num_end] == '.' || json[num_end] == 'e' || json[num_end] == 'E'))
        num_end++;

    if (num_start == num_end) return 0.0f;

    luisa::string num_str = json.substr(num_start, num_end - num_start);
    return std::atof(num_str.c_str());
}

luisa::string SceneSync::json_get_object(const luisa::string &json, const char *key) {
    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return "";

    size_t brace_start = json.find('{', colon_pos);
    if (brace_start == luisa::string::npos) return "";

    // Find matching closing brace
    int brace_count = 1;
    size_t pos = brace_start + 1;
    while (pos < json.length() && brace_count > 0) {
        if (json[pos] == '{')
            brace_count++;
        else if (json[pos] == '}')
            brace_count--;
        pos++;
    }

    if (brace_count != 0) return "";

    return json.substr(brace_start, pos - brace_start);
}

luisa::vector<luisa::string> SceneSync::json_get_array_objects(const luisa::string &json, const char *key) {
    luisa::vector<luisa::string> result;

    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return result;

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return result;

    size_t bracket_start = json.find('[', colon_pos);
    if (bracket_start == luisa::string::npos) return result;

    size_t bracket_end = json.find(']', bracket_start);
    if (bracket_end == luisa::string::npos) return result;

    // Extract objects from array
    size_t pos = bracket_start + 1;
    while (pos < bracket_end) {
        // Skip whitespace
        while (pos < bracket_end && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == ','))
            pos++;

        if (pos >= bracket_end) break;

        if (json[pos] == '{') {
            size_t obj_start = pos;
            int brace_count = 1;
            pos++;
            while (pos < bracket_end && brace_count > 0) {
                if (json[pos] == '{')
                    brace_count++;
                else if (json[pos] == '}')
                    brace_count--;
                pos++;
            }
            result.emplace_back(json.substr(obj_start, pos - obj_start));
        } else {
            pos++;
        }
    }

    return result;
}

luisa::vector<float> SceneSync::json_get_float_array(const luisa::string &json, const char *key) {
    luisa::vector<float> result;

    luisa::string search_key = luisa::string("\"") + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == luisa::string::npos) return result;

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == luisa::string::npos) return result;

    size_t bracket_start = json.find('[', colon_pos);
    if (bracket_start == luisa::string::npos) return result;

    size_t bracket_end = json.find(']', bracket_start);
    if (bracket_end == luisa::string::npos) return result;

    // Parse numbers
    size_t pos = bracket_start + 1;
    while (pos < bracket_end) {
        // Skip whitespace and commas
        while (pos < bracket_end && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ',' || json[pos] == '\n'))
            pos++;

        if (pos >= bracket_end) break;

        // Parse number
        size_t num_start = pos;
        while (pos < bracket_end && (std::isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+'))
            pos++;

        if (num_start != pos) {
            luisa::string num_str = json.substr(num_start, pos - num_start);
            result.push_back(std::atof(num_str.c_str()));
        }
    }

    return result;
}

bool SceneSync::parse_scene_state(const luisa::string &json) {
    // Extract scene object
    luisa::string scene_obj = json_get_object(json, "scene");
    if (scene_obj.empty()) {
        LUISA_WARNING("No scene object found in JSON");
        return false;
    }

    // Get entities array
    auto entity_objects = json_get_array_objects(scene_obj, "entities");
    if (entity_objects.empty()) {
        LUISA_INFO("No entities in scene");
        entities_.clear();
        entity_map_.clear();
        return true;
    }

    // Parse entities
    luisa::vector<SceneEntity> new_entities;
    for (const auto &entity_json : entity_objects) {
        SceneEntity entity = parse_entity(entity_json);
        new_entities.emplace_back(entity);
    }

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
}

SceneEntity SceneSync::parse_entity(const luisa::string &json_object) {
    SceneEntity entity;
    entity.id = json_get_int(json_object, "id");
    entity.name = json_get_string(json_object, "name");

    // Get components object
    luisa::string components_obj = json_get_object(json_object, "components");
    if (!components_obj.empty()) {
        // Parse transform
        luisa::string transform_obj = json_get_object(components_obj, "transform");
        if (!transform_obj.empty()) {
            entity.transform = parse_transform(transform_obj);
        }

        // Parse render component
        luisa::string render_obj = json_get_object(components_obj, "render");
        if (!render_obj.empty()) {
            entity.render_component = parse_render_component(render_obj);
            entity.has_render_component = true;
        }
    }

    return entity;
}

Transform SceneSync::parse_transform(const luisa::string &json_object) {
    Transform transform;

    auto pos = json_get_float_array(json_object, "position");
    if (pos.size() >= 3) {
        transform.position = luisa::float3(pos[0], pos[1], pos[2]);
    }

    auto rot = json_get_float_array(json_object, "rotation");
    if (rot.size() >= 4) {
        transform.rotation = luisa::float4(rot[0], rot[1], rot[2], rot[3]);
    }

    auto scale = json_get_float_array(json_object, "scale");
    if (scale.size() >= 3) {
        transform.scale = luisa::float3(scale[0], scale[1], scale[2]);
    }

    return transform;
}

RenderComponent SceneSync::parse_render_component(const luisa::string &json_object) {
    RenderComponent render;
    render.mesh_id = json_get_int(json_object, "mesh_id");
    // Material IDs can be parsed similarly if needed
    return render;
}

bool SceneSync::parse_resources(const luisa::string &json) {
    // Get resources array
    auto resource_objects = json_get_array_objects(json, "resources");
    if (resource_objects.empty()) {
        LUISA_INFO("No resources found");
        return false;
    }

    // Parse resources
    luisa::vector<EditorResourceMetadata> new_resources;
    for (const auto &resource_json : resource_objects) {
        EditorResourceMetadata resource = parse_resource(resource_json);
        new_resources.emplace_back(resource);
    }

    // Check if resources changed
    bool changed = (new_resources.size() != resources_.size());

    // Update resources
    resources_ = new_resources;
    resource_map_.clear();
    for (size_t i = 0; i < resources_.size(); ++i) {
        resource_map_[resources_[i].id] = i;
    }

    return changed;
}

EditorResourceMetadata SceneSync::parse_resource(const luisa::string &json_object) {
    EditorResourceMetadata resource;
    resource.id = json_get_int(json_object, "id");
    resource.type = json_get_int(json_object, "type");
    resource.path = json_get_string(json_object, "path");
    resource.state = json_get_int(json_object, "state");
    resource.size_bytes = json_get_int(json_object, "size_bytes");
    return resource;
}

}// namespace rbc
