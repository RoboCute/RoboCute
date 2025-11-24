#pragma once
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/optional.h>
#include <luisa/vstl/common.h>
#include <rbc_config.h>
#include <rbc_core/serde.h>
#include <rbc_core/heap_object.h>
namespace rbc {
struct RBC_CORE_API FuncSerializer {
    using ClousureType = vstd::func_ptr_t<void(void *self, void *args, void *ret_value)>;
    struct FuncCall : vstd::IOperatorNewBase {
        ClousureType func;
        HeapObjectMeta args_meta;
        HeapObjectMeta ret_value_meta;
        FuncCall(
            ClousureType const &func,
            HeapObjectMeta const &args_meta,
            HeapObjectMeta const &ret_value_meta)
            : func(func), args_meta(args_meta), ret_value_meta(ret_value_meta) {
        }
    };
    FuncSerializer(
        std::initializer_list<const char *> names,
        std::initializer_list<ClousureType> funcs,
        std::initializer_list<HeapObjectMeta> args_meta,
        std::initializer_list<HeapObjectMeta> ret_value_meta);
    ~FuncSerializer();
    static FuncCall const* get_call_meta(luisa::string_view name);
    static bool try_call(
        luisa::string_view name,
        void *self,
        void *args,
        void *ret_value);
};
}// namespace rbc