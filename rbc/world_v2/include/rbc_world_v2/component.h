#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Entity;
struct EntityImpl;
// events from front to back
enum struct FrameTick {
    BeforeSimulation,
    BeforeRendering,
    OnFrameEnd,
    NUM
};
struct ComponentBase : BaseObject {
    friend struct Entity;
    friend struct EntityImpl;
    template<typename T>
    friend struct ComponentDerive;
protected:
    Entity *_entity{};
private:
    virtual void _remove_self_from_entity() = 0;
public:
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Component;
    [[nodiscard]] Entity *entity() const {
        return _entity;
    }
    [[nodiscard]] BaseObjectType base_type() const override {
        return BaseObjectType::Component;
    }
};
struct Component : ComponentBase {
    friend struct Entity;
    friend struct EntityImpl;
private:// for internal usage
    void _remove_self_from_entity() override;
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
    ~ComponentDerive() {
        static_cast<ComponentBase *>(this)->_remove_self_from_entity();
        static_cast<BaseObjectBase *>(this)->_dispose_self();    
    }
};
}// namespace rbc::world