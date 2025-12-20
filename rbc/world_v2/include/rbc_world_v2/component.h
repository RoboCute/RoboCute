#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
enum struct WorldEventType : uint64_t {
    OnTransformUpdate
    // etc...
    // can have custom events
};
struct Entity;
// events from front to back
struct Component : BaseObject {
    friend struct Entity;
    template<typename T>
    friend struct ComponentDerive;
private:
    Entity *_entity{};
    RBC_WORLD_API void _remove_self_from_entity();
    explicit Component(Entity *entity);
    ~Component();
    RBC_WORLD_API void add_event(WorldEventType event, void (Component::*func_ptr)());
    RBC_WORLD_API void _clear_entity();
protected:
    RBC_WORLD_API void remove_event(WorldEventType event);
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
    void add_event(WorldEventType event, void (T::*func_ptr)()) {
        Component::add_event(event, static_cast<void (Component::*)()>(func_ptr));
    }
};
}// namespace rbc::world