#pragma once
#include <rbc_world/base_object.h>
#include <rbc_core/coroutine.h>
namespace rbc::world {
struct Entity;
enum struct WorldEventType {
    BeforeFrame,
    BeforeRender,
    AfterFrame,
};
static constexpr size_t world_event_count = luisa::to_underlying(WorldEventType::AfterFrame) + 1;
// events from front to back
struct RBC_RUNTIME_API Component : BaseObject {
    friend struct Entity;
    template<typename T>
    friend struct ComponentDerive;
private:
    Entity *_entity{};
    void _remove_self_from_entity();
    explicit Component(Entity *entity);
    ~Component();

    void _clear_entity();
public:
    // should only be called internally
    static void _zz_invoke_world_event(WorldEventType event_type);
    void add_world_event(WorldEventType event_type, rbc::coroutine &&coro);
    void remove_world_event(WorldEventType event_type);
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
    [[nodiscard]] MD5 type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
protected:
    explicit ComponentDerive(Entity *entity) : Component(entity) {}
    ~ComponentDerive() {
        Component::_remove_self_from_entity();
    }
};
}// namespace rbc::world