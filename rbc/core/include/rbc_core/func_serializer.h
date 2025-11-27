#pragma once
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/optional.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_config.h>
#include <rbc_core/serde.h>
#include <rbc_core/heap_object.h>
namespace rbc {
struct RBC_CORE_API FuncSerializer {
    using AnyFuncPtr = vstd::func_ptr_t<void()>;
    using StaticFuncType = vstd::func_ptr_t<void(void *args, void *ret_value)>;
    using ClousureType = vstd::func_ptr_t<void(void *self, void *args, void *ret_value)>;
    struct FuncCall : vstd::IOperatorNewBase {
        AnyFuncPtr func;
        HeapObjectMeta args_meta;
        HeapObjectMeta ret_value_meta;
        bool is_static;
        FuncCall(
            AnyFuncPtr const &func,
            HeapObjectMeta const &args_meta,
            HeapObjectMeta const &ret_value_meta,
            bool is_static)
            : func(func), args_meta(args_meta), ret_value_meta(ret_value_meta), is_static(is_static) {
        }
    };
    FuncSerializer(
        std::initializer_list<const char *> names,
        std::initializer_list<AnyFuncPtr> funcs,
        std::initializer_list<HeapObjectMeta> args_meta,
        std::initializer_list<HeapObjectMeta> ret_value_meta,
        std::initializer_list<bool> is_static);
    ~FuncSerializer();
    static FuncCall const *get_call_meta(vstd::Guid const &name);
};
}// namespace rbc