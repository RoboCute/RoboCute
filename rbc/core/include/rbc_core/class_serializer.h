#pragma once
#include <rbc_config.h>
#include <rbc_core/serde.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/md5.h>
namespace rbc {
struct RBC_CORE_API ClassSerializer {
    using SerFunc = vstd::func_ptr_t<void(void const *, ::rbc::JsonSerializer *)>;
    using DeserFunc = vstd::func_ptr_t<void(void *, ::rbc::JsonDeSerializer *)>;
    struct SerdeFuncs {
        SerFunc rbc_objser{};
        DeserFunc rbc_objdeser{};
    };
    ClassSerializer(
        vstd::MD5 type_md5,
        SerdeFuncs serde_func);
    ~ClassSerializer();
    static SerdeFuncs get_serde_funcs(vstd::MD5 type_md5);
    template<concepts::RTTIType T>
    inline static SerdeFuncs get_serde_funcs() {
        return get_serde_funcs(TypeInfo::template get<T>().md5());
    }
    template<concepts::RTTIType T>
        requires(
            ::rbc::detail::serializable_struct_type_v<T, ::rbc::JsonSerializer> &&
            ::rbc::detail::deserializable_struct_type_v<T, ::rbc::JsonDeSerializer>)
    static SerdeFuncs _zz_create_serde_funcs() {
        SerdeFuncs r;
        if constexpr (requires { std::declval<T>().rbc_objser(lvalue_declval<::rbc::JsonSerializer>()); }) {
            r.rbc_objser = +[](void const *ptr, ::rbc::JsonSerializer *ser) {
                static_cast<T const *>(ptr)->rbc_objser(*ser);
            };
        } else {
            r.rbc_objser = +[](void const *ptr, ::rbc::JsonSerializer *ser) {
                static_cast<T const *>(ptr)->rbc_arrser(*ser);
            };
        }

        if constexpr (requires { std::declval<T>().rbc_objdeser(lvalue_declval<::rbc::JsonDeSerializer>()); }) {
            r.rbc_objdeser = +[](void *ptr, ::rbc::JsonDeSerializer *ser) {
                static_cast<T *>(ptr)->rbc_objdeser(*ser);
            };
        } else {
            r.rbc_objdeser = +[](void *ptr, ::rbc::JsonDeSerializer *ser) {
                static_cast<T *>(ptr)->rbc_arrdeser(*ser);
            };
        }
        return r;
    }
};
}// namespace rbc
#define RBC_DECLARE_SERDE_CLASS(class_name) static ::rbc::ClassSerializer n##class_name{::rbc::TypeInfo::template get<class_name>().md5(), ::rbc::ClassSerializer::template _zz_create_serde_funcs<class_name>()};