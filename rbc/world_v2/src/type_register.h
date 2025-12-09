#pragma once
#include <rbc_world_v2/base_object.h>
#include <luisa/vstl/pool.h>
namespace rbc::world {
template<typename T, typename Impl>
concept RegistableWorldObject = std::is_base_of_v<BaseObject, T> && std::is_default_constructible_v<Impl> && std::is_base_of_v<T, Impl>;
struct TypeRegisterBase;
using CreateFunc = vstd::func_ptr_t<BaseObject *()>;
struct TypeRegisterBase {
    std::array<uint64_t, 2> type_id;
    CreateFunc create_func;
    TypeRegisterBase *p_next{};
};
void type_regist_init_mark(TypeRegisterBase *type_register);
template<typename T, typename Impl>
    requires RegistableWorldObject<T, Impl>
struct TypeRegister : TypeRegisterBase {
    TypeRegister() {
        type_id = rbc_rtti_detail::is_rtti_type<T>::get_md5();
        create_func = +[]() -> BaseObject * {
            return new Impl{};
        };
        type_regist_init_mark(this);
    }
};
}// namespace rbc::world