#pragma once
#include <luisa/core/stl/string.h>

namespace rbc {
template<typename T>
struct is_rtti_type {
    static constexpr bool value = false;
};
template<typename T>
static constexpr bool is_rtti_type_v = is_rtti_type<T>::value;
}// namespace rbc

// example:
// namespace rbc {
// template<>
// struct is_rtti_type<MyType> {
//     static constexpr bool value = true;
//     static constexpr luisa::string_view name{"type"};
//     static constexpr uint64_t md5[2]{114, 514};
// };
// }