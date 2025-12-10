#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Entity;
struct EntityImpl;
struct Component : BaseObject {
    friend struct EntityImpl;
protected:
    Entity *_entity{};
public:
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Component;
    [[nodiscard]] Entity *entity() const {
        return _entity;
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
    [[nodiscard]] BaseObjectType base_type() const  override{
        return BaseObjectType::Component;
    }
};
}// namespace rbc::world