#pragma once
#include <rbc_core/type_info.h>
#include <rbc_world_v2/component.h>
#include <luisa/vstl/ranges.h>
namespace rbc::world {
struct Component;

struct EntityCompIter {
    using IterType = luisa::unordered_map<std::array<uint64_t, 2>, Component *>::const_iterator;
private:
    IterType _iter;
public:
    EntityCompIter(IterType iter) : _iter(iter) {}
    Component *operator*() {
        return _iter->second;
    }
    Component *operator*() const {
        return _iter->second;
    }
    void operator++() {
        _iter++;
    }
    bool operator==(auto const &iter) const {
        return iter == _iter;
    }
};
struct Entity final : BaseObjectDerive<Entity, BaseObjectType::Entity> {
    DECLARE_WORLD_OBJECT_FRIEND(Entity)
    friend struct Component;

private:
    luisa::unordered_map<std::array<uint64_t, 2>, Component *> _components;
    luisa::unordered_map<
        WorldEventType,
        luisa::unordered_map<
            Component *,
            void (Component::*)()>>
        _events;
    RBC_WORLD_API void _remove_component(Component *component);
    Entity() = default;
    ~Entity();
public:
    RBC_WORLD_API void dispose() override;
    EntityCompIter begin() const {
        return EntityCompIter{_components.begin()};
    }
    auto end() const {
        return _components.end();
    }
    uint64_t component_count() const {
        return _components.size();
    }
    RBC_WORLD_API void add_component(Component *component);
    template<typename T>
        requires(std::is_base_of_v<Component, T>)
    T* add_component() {
        auto ptr = create_object<T>();
        add_component(ptr);
        return ptr;
    }
    RBC_WORLD_API bool remove_component(TypeInfo const &type);
    RBC_WORLD_API Component *get_component(TypeInfo const &type);
    RBC_WORLD_API void rbc_objser(rbc::JsonSerializer &ser) const override;
    RBC_WORLD_API void rbc_objdeser(rbc::JsonDeSerializer &ser) override;
    RBC_WORLD_API void broadcast_event(WorldEventType frame_tick);
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<Component, T>)
    bool remove_component() {
        return remove_component(TypeInfo::get<T>());
    }
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<Component, T>)
    T *get_component() {
        return static_cast<T *>(get_component(TypeInfo::get<T>()));
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);