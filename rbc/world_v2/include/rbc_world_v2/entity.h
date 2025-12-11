#pragma once
#include <rbc_core/type_info.h>
#include <rbc_world_v2/base_object.h>
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
struct Entity : BaseObjectDerive<Entity, BaseObjectType::Entity> {
    friend struct Component;
    friend struct EntityImpl;

private:
    luisa::unordered_map<std::array<uint64_t, 2>, Component *> _components;
    void _remove_component(Component *component);
    Entity() = default;
    ~Entity() = default;
public:
    EntityCompIter begin() const {
        return EntityCompIter{_components.begin()};
    }
    auto end() const {
        return _components.end();
    }
    uint64_t component_count() const {
        return _components.size();
    }
    virtual bool add_component(Component *component) = 0;
    virtual bool remove_component(TypeInfo const &type) = 0;
    virtual Component *get_component(TypeInfo const &type) = 0;
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