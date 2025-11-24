#pragma once

#include "rbc_config.h"
#include <luisa/core/stl.h>
#include <utility>

namespace rbc {

using EntityID = uint32_t;
constexpr EntityID INVALID_ENTITY = 0;
using ComponentTypeID = uint32_t;

// Basic Component Interface
class IComponent {
public:
    virtual ~IComponent() = default;
    [[nodiscard]] virtual ComponentTypeID get_type_id() const = 0;
    [[nodiscard]] virtual luisa::unique_ptr<IComponent> clone() const = 0;
};

// Component Array Interface for type erasure
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void entity_destroyed(EntityID entity) = 0;
    [[nodiscard]] virtual bool has_component(EntityID entity) const = 0;
    virtual void remove_component(EntityID entity) = 0;
};

// Templated Component Array
template<typename T>
class ComponentArray : public IComponentArray {
public:
    void insert(EntityID entity, T component) {
        if (_entity_to_index.contains(entity)) {
            // Replace existing component
            size_t index = _entity_to_index[entity];
            _components[index] = std::move(component);
        } else {
            size_t new_index = _size;
            _entity_to_index[entity] = new_index;
            _index_to_entity[new_index] = entity;
            if (new_index >= _components.size()) {
                _components.resize(new_index + 1);
            }
            _components[new_index] = std::move(component);
            ++_size;
        }
    }

    void remove_component(EntityID entity) override {
        if (!_entity_to_index.contains(entity)) {
            return;
        }

        // Move last element into deleted element's place to maintain density
        size_t removed_index = _entity_to_index[entity];
        size_t last_index = _size - 1;

        if (removed_index != last_index) {
            _components[removed_index] = std::move(_components[last_index]);
            EntityID last_entity = _index_to_entity[last_index];
            _entity_to_index[last_entity] = removed_index;
            _index_to_entity[removed_index] = last_entity;
        }

        _entity_to_index.erase(entity);
        _index_to_entity.erase(last_index);
        --_size;
    }

    [[nodiscard]] T *get(EntityID entity) {
        if (!_entity_to_index.contains(entity)) {
            return nullptr;
        }
        return &_components[_entity_to_index[entity]];
    }

    [[nodiscard]] const T *get(EntityID entity) const {
        auto it = _entity_to_index.find(entity);
        if (it == _entity_to_index.end()) {
            return nullptr;
        }
        return &_components[it->second];
    }

    void entity_destroyed(EntityID entity) override {
        remove_component(entity);
    }

    [[nodiscard]] bool has_component(EntityID entity) const override {
        return _entity_to_index.contains(entity);
    }

    [[nodiscard]] luisa::vector<EntityID> get_all_entities() const {
        luisa::vector<EntityID> entities;
        entities.reserve(_size);
        for (const auto &[entity, index] : _entity_to_index) {
            entities.push_back(entity);
        }
        return entities;
    }

private:
    luisa::vector<T> _components;                           // actual component storage
    luisa::unordered_map<EntityID, size_t> _entity_to_index;// lookup table from entity to local index
    luisa::unordered_map<size_t, EntityID> _index_to_entity;// loopup table from local index to entity
    size_t _size = 0;
};

// Component Registry
class RBC_WORLD_API ComponentRegistry {
public:
    template<typename T>
    static ComponentTypeID register_type(const char *name) {
        static ComponentTypeID id = next_component_type_id();
        register_name(id, name);
        return id;
    }

    [[nodiscard]] static const char *get_component_name(ComponentTypeID id);
    [[nodiscard]] static ComponentTypeID get_component_id(const char *name);

private:
    static ComponentTypeID next_component_type_id();
    static void register_name(ComponentTypeID id, const char *name);
};

// Entity Manager
class RBC_WORLD_API EntityManager {
public:
    EntityManager();
    ~EntityManager() = default;

    // Delete copy and move to prevent issues with unique_ptr members
    EntityManager(const EntityManager &) = delete;
    EntityManager(EntityManager &&) = delete;
    EntityManager &operator=(const EntityManager &) = delete;
    EntityManager &operator=(EntityManager &&) = delete;

    // Entity operations
    EntityID create_entity();
    void destroy_entity(EntityID entity);
    [[nodiscard]] bool is_valid(EntityID entity) const;

    // Component operations
    template<typename T, typename... Args>
    T &add_component(EntityID entity, Args &&...args) {
        if (!is_valid(entity)) {
            LUISA_ERROR("Attempting to add component to invalid entity");
        }

        ComponentTypeID type_id = T::get_static_type_id();
        if (!_component_arrays.contains(type_id)) {
            _component_arrays[type_id] = luisa::make_unique<ComponentArray<T>>();
        }

        auto *array = static_cast<ComponentArray<T> *>(_component_arrays[type_id].get());
        T component(std::forward<Args>(args)...);
        array->insert(entity, std::move(component));
        return *array->get(entity);
    }

    template<typename T>
    T *get_component(EntityID entity) {
        if (!is_valid(entity)) {
            return nullptr;
        }

        ComponentTypeID type_id = T::get_static_type_id();
        if (!_component_arrays.contains(type_id)) {
            return nullptr;
        }

        auto *array = static_cast<ComponentArray<T> *>(_component_arrays[type_id].get());
        return array->get(entity);
    }

    template<typename T>
    const T *get_component(EntityID entity) const {
        if (!is_valid(entity)) {
            return nullptr;
        }

        ComponentTypeID type_id = T::get_static_type_id();
        auto it = _component_arrays.find(type_id);
        if (it == _component_arrays.end()) {
            return nullptr;
        }

        auto *array = static_cast<ComponentArray<T> *>(it->second.get());
        return array->get(entity);
    }

    template<typename T>
    void remove_component(EntityID entity) {
        if (!is_valid(entity)) {
            return;
        }

        ComponentTypeID type_id = T::get_static_type_id();
        if (!_component_arrays.contains(type_id)) {
            return;
        }

        auto *array = static_cast<ComponentArray<T> *>(_component_arrays[type_id].get());
        array->remove_component(entity);
    }

    template<typename T>
    [[nodiscard]] bool has_component(EntityID entity) const {
        if (!is_valid(entity)) {
            return false;
        }

        ComponentTypeID type_id = T::get_static_type_id();
        auto it = _component_arrays.find(type_id);
        if (it == _component_arrays.end()) {
            return false;
        }

        return it->second->has_component(entity);
    }

    template<typename T>
    [[nodiscard]] luisa::vector<EntityID> get_entities_with_component() const {
        ComponentTypeID type_id = T::get_static_type_id();
        auto it = _component_arrays.find(type_id);
        if (it == _component_arrays.end()) {
            return {};
        }

        auto *array = static_cast<ComponentArray<T> *>(it->second.get());
        return array->get_all_entities();
    }

    // Get total entity count
    [[nodiscard]] size_t get_entity_count() const;

    // Get all valid entities
    [[nodiscard]] luisa::vector<EntityID> get_all_entities() const;

private:
    luisa::vector<bool> _entity_valid;
    luisa::queue<EntityID> _free_entities;
    EntityID _next_entity_id = 1;
    luisa::unordered_map<ComponentTypeID, luisa::unique_ptr<IComponentArray>> _component_arrays;
};

}// namespace rbc

#define DECLARE_COMPONENT(ComponentClass)                                           \
    static rbc::ComponentTypeID get_static_type_id() {                              \
        static rbc::ComponentTypeID id =                                            \
            rbc::ComponentRegistry::register_type<ComponentClass>(#ComponentClass); \
        return id;                                                                  \
    }                                                                               \
    rbc::ComponentTypeID get_type_id() const override { return get_static_type_id(); }
