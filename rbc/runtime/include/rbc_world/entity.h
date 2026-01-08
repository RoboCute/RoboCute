#pragma once
#include <rbc_core/type_info.h>
#include <rbc_core/serde.h>
#include <rbc_world/component.h>
#include <luisa/vstl/ranges.h>
namespace rbc::world {
struct Component;
namespace detail {
}
struct EntityCompIter {
    using IterType = luisa::unordered_map<MD5, Component *>::const_iterator;
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
struct RBC_RUNTIME_API Entity final : BaseObjectDerive<Entity, BaseObjectType::Entity> {
    DECLARE_WORLD_OBJECT_FRIEND(Entity)
    friend struct Component;

private:
    luisa::unordered_map<MD5, Component *> _components;
    void _remove_component(Component *component);
    Entity();
    ~Entity();
public:
    Component *_create_component(MD5 const &type);
    void _add_component(Component *component);
    EntityCompIter begin() const {
        return EntityCompIter{_components.begin()};
    }
    auto end() const {
        return _components.end();
    }
    uint64_t component_count() const {
        return _components.size();
    }
    template<typename T>
        requires(std::is_base_of_v<Component, T>)
    T *add_component() {
        auto ptr = _create_component(rbc_rtti_detail::is_rtti_type<T>::get_md5());
        _add_component(ptr);
        return static_cast<T *>(ptr);
    }
    bool remove_component(MD5 const &type_md5);
    Component *get_component(MD5 const &type_md5);
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<Component, T>)
    bool remove_component() {
        return remove_component(TypeInfo::get<T>().md5());
    }
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<Component, T>)
    T *get_component() {
        return static_cast<T *>(get_component(TypeInfo::get<T>().md5()));
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);

template<>
struct rbc::Serialize<rbc::world::Entity> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::world::Entity &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::world::Entity &v);
};