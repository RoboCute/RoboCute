#pragma once
#include <luisa/core/stl/string.h>
#include <array>
#include <luisa/vstl/md5.h>
namespace rbc_rtti_detail {
template<typename T>
struct is_rtti_type {
    static constexpr bool value = false;
};
}// namespace rbc_rtti_detail
namespace rbc {

template<typename T>
static constexpr bool is_rtti_type_v = rbc_rtti_detail::is_rtti_type<T>::value;
}// namespace rbc
#define RBC_RTTI(ClassName)                                          \
    namespace rbc_rtti_detail {                                      \
    template<>                                                       \
    struct is_rtti_type<ClassName> {                                 \
        static constexpr bool value = true;                          \
        static constexpr const char *name{#ClassName};               \
        static std::array<uint64_t, 2> get_md5() {                   \
            static vstd::MD5 _md5{luisa::string_view{name}};         \
            return {_md5.to_binary().data0, _md5.to_binary().data1}; \
        }                                                            \
    };                                                               \
    }
