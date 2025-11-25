#include "rbc_world/ecs.h"
#include <luisa/core/logging.h>

namespace rbc {

// ComponentRegistry implementation
static luisa::unordered_map<ComponentTypeID, luisa::string> &get_id_to_name_map() {
    static luisa::unordered_map<ComponentTypeID, luisa::string> map;
    return map;
}

static luisa::unordered_map<luisa::string, ComponentTypeID> &get_name_to_id_map() {
    static luisa::unordered_map<luisa::string, ComponentTypeID> map;
    return map;
}

ComponentTypeID ComponentRegistry::next_component_type_id() {
    static ComponentTypeID next_id = 1;
    return next_id++;
}

void ComponentRegistry::register_name(ComponentTypeID id, const char *name) {
    auto &id_to_name = get_id_to_name_map();
    auto &name_to_id = get_name_to_id_map();

    luisa::string name_str(name);
    id_to_name[id] = name_str;
    name_to_id[name_str] = id;
}

const char *ComponentRegistry::get_component_name(ComponentTypeID id) {
    auto &id_to_name = get_id_to_name_map();
    auto it = id_to_name.find(id);
    if (it != id_to_name.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

ComponentTypeID ComponentRegistry::get_component_id(const char *name) {
    auto &name_to_id = get_name_to_id_map();
    luisa::string name_str(name);
    auto it = name_to_id.find(name_str);
    if (it != name_to_id.end()) {
        return it->second;
    }
    return 0;
}

// EntityManager implementation
EntityManager::EntityManager() {
    // Reserve some space for entities
    _entity_valid.reserve(1000);
}

EntityID EntityManager::create_entity() {
    EntityID entity_id;

    if (!_free_entities.empty()) {
        // Reuse a freed entity ID
        entity_id = _free_entities.front();
        _free_entities.pop();
        _entity_valid[entity_id - 1] = true;
    } else {
        // Create a new entity ID
        entity_id = _next_entity_id++;

        // Expand the valid array if needed
        if (entity_id > _entity_valid.size()) {
            _entity_valid.resize(entity_id, false);
        }
        _entity_valid[entity_id - 1] = true;
    }

    return entity_id;
}

void EntityManager::destroy_entity(EntityID entity) {
    if (!is_valid(entity)) {
        return;
    }

    // Mark entity as invalid
    _entity_valid[entity - 1] = false;

    // Remove all components associated with this entity
    for (auto &[type_id, array] : _component_arrays) {
        array->entity_destroyed(entity);
    }

    // Add to free list for reuse
    _free_entities.push(entity);
}

bool EntityManager::is_valid(EntityID entity) const {
    if (entity == INVALID_ENTITY || entity == 0) {
        return false;
    }

    if (entity > _entity_valid.size()) {
        return false;
    }

    return _entity_valid[entity - 1];
}

size_t EntityManager::get_entity_count() const {
    size_t count = 0;
    for (bool valid : _entity_valid) {
        if (valid) {
            ++count;
        }
    }
    return count;
}

luisa::vector<EntityID> EntityManager::get_all_entities() const {
    luisa::vector<EntityID> entities;
    entities.reserve(get_entity_count());

    for (size_t i = 0; i < _entity_valid.size(); ++i) {
        if (_entity_valid[i]) {
            entities.push_back(static_cast<EntityID>(i + 1));
        }
    }

    return entities;
}

}// namespace rbc