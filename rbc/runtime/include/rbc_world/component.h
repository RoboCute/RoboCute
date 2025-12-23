#pragma once
#include <rbc_world/base_object.h>
namespace rbc::world {
struct Entity;
// events from front to back
struct Component : BaseObject {
    friend struct Entity;
    template<typename T>
    friend struct ComponentDerive;
private:
    Entity *_entity{};
    RBC_RUNTIME_API void _remove_self_from_entity();
    explicit Component(Entity *entity);
    ~Component();
    
    RBC_RUNTIME_API void _clear_entity();
protected:
public:
    virtual void on_awake() {};
    virtual void on_destroy() {};
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Component;
    [[nodiscard]] Entity *entity() const {
        return _entity;
    }
    [[nodiscard]] BaseObjectType base_type() const override {
        return BaseObjectType::Component;
    }
};
template<typename T>
struct ComponentDerive : Component {
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
protected:
    explicit ComponentDerive(Entity *entity) : Component(entity) {}
    ~ComponentDerive() {
        Component::_remove_self_from_entity();
    }

};
}// namespace rbc::world