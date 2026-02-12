#pragma once
#include <rbc_world/base_object.h>
#include <rbc_core/coroutine.h>
namespace rbc::world {
struct Resource;
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
    std::atomic_bool _enabled{false};
    Component();
    ~Component();

    void _clear_entity();
    void _call_on_awake();
    void _call_on_destroy();
public:
    bool enabled() const { return _enabled; }
    // should only be called internally
    void remove_self_from_entity();
    static void _zz_invoke_world_event(WorldEventType event_type);
    void add_world_event(WorldEventType event_type, rbc::coroutine &&coro);
    void remove_world_event(WorldEventType event_type);
    ////////// for reload

    virtual void update_data() {}
    ////////// for reload

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
    ComponentDerive() = default;
    ~ComponentDerive() {
    }
};
}// namespace rbc::world