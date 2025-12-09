#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct RBC_RUNTIME_API TypeRegister {
    static void init_instance();
    static void destroy_instance();
    using CreateFunc = vstd::func_ptr_t<BaseObject *()>;
    TypeRegister(
        std::array<uint64_t, 2> guid,
        CreateFunc create_obj);
};
}// namespace rbc::world