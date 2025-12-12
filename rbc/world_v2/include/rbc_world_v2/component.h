#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Entity;
// events from front to back
enum struct FrameTick {
    BeforeSimulation,
    BeforeRendering,
    OnFrameEnd,
    NUM
};

struct Component : BaseObject {
    friend struct Entity;
    template<typename T>
    friend struct ComponentDerive;
protected:
    Entity *_entity{};
    RBC_WORLD_API void _remove_self_from_entity();
    Component() = default;
    ~Component() = default;
public:
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
    ComponentDerive() = default;
    virtual ~ComponentDerive() {
        Component::_remove_self_from_entity();
    }
};
}// namespace rbc::world