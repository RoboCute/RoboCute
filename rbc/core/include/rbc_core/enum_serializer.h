#pragma once
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/optional.h>
#include <luisa/vstl/meta_lib.h>
#include <initializer_list>
#include <rbc_config.h>
#include <rbc_core/base.h>
#include <rbc_core//type_info.h>

namespace rbc {
struct EnumReflectionType {
    luisa::string enum_name;
    uint64_t value;
};
struct EnumReflectionTypeHasher {
    uint64_t operator()(EnumReflectionType const &a) const {
        return luisa::hash<luisa::string>{}(a.enum_name, luisa::hash<uint64_t>{}(a.value));
    }
    bool operator()(EnumReflectionType const &a, EnumReflectionType const &b) const {
        return a.enum_name == b.enum_name && a.value == b.value;
    }
};
struct RBC_CORE_API EnumSerializer {
    // key: {namespace::EnumType, enum)value}   value:      EnumValueName
    luisa::unordered_map<EnumReflectionType, luisa::string, EnumReflectionTypeHasher, EnumReflectionTypeHasher> enum_value_to_name;
    // key: namespace::EnumType##Value          value:      enum_value
    luisa::unordered_map<luisa::string, uint64_t> enum_name_to_value;
    luisa::optional<uint32_t> get_value(char const *name);

    static luisa::string_view _get_enum_value_name(
        luisa::string_view enum_name,
        uint64_t value);

    static vstd::optional<uint64_t> _get_enum_value(
        luisa::string_view enum_type,
        luisa::string_view enum_value);
    template<typename T>
        requires(std::is_enum_v<T>)
    static vstd::optional<uint64_t> get_enum_value(
        luisa::string_view enum_value) {
        return _get_enum_value(rbc_rtti_detail::is_rtti_type<T>::name, enum_value);
    }
    template<typename T>
        requires(std::is_enum_v<T>)
    static luisa::string_view get_enum_value_name(
        uint64_t value) {
        return _get_enum_value_name(rbc_rtti_detail::is_rtti_type<T>::name, value);
    }
};
struct RBC_CORE_API EnumSerIniter {
    EnumSerIniter(
        luisa::string_view enun_name,
        std::initializer_list<char const *> names,
        std::initializer_list<uint64_t> numbers);
};
}// namespace rbc