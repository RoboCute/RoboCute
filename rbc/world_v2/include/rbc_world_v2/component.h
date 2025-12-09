#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Entity;
struct Component : BaseObjectImpl {
    friend struct Entity;
protected:
    Entity *_entity;
public:
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
};
}// namespace rbc::world