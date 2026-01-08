#pragma once
#include <rbc_config.h>
#include <luisa/core//stl/string.h>
#include <luisa/core//stl/hash.h>
#include <luisa/vstl/hash.h>
#include <luisa/vstl/md5.h>
#include <luisa/vstl/compare.h>
#include <type_traits>
#include <rbc_core/base.h>
#include <cstdint>
namespace rbc {
using MD5 = vstd::MD5;
}// namespace rbc
namespace rbc_rtti_detail {
template<typename T>
struct is_rtti_type {
    static constexpr bool value = false;
    static_assert(luisa::always_false_v<T>, "Invalid RTTI, did you forget declare \"RBC_RTTI(TT)\"");
    static constexpr const char *name = "";
    static rbc::MD5 get_md5() {
        return {};
    }
};
}// namespace rbc_rtti_detail

namespace rbc {

template<typename T>
static constexpr bool is_rtti_type_v = rbc_rtti_detail::is_rtti_type<T>::value;
#define RBC_RTTI(ClassName)                                  \
    namespace rbc_rtti_detail {                              \
    template<>                                               \
    struct is_rtti_type<ClassName> {                         \
        static constexpr bool value = true;                  \
        static constexpr const char *name{#ClassName};       \
        static rbc::MD5 get_md5() {                    \
            static vstd::MD5 _md5{luisa::string_view{name}}; \
            return _md5;                                     \
        }                                                    \
    };                                                       \
    }

template<class _Ty>
static _Ty &lvalue_declval() noexcept {
    static_assert(false, "Calling declval is ill-formed, see N4950 [declval]/2.");
}
namespace concepts {
template<typename T>
concept RTTIType = is_rtti_type_v<T>;
}// namespace concepts
struct TypeInfo {
private:
    luisa::string_view _name;
    MD5 _type_index;
public:
    TypeInfo(luisa::string_view name, MD5 const &type_index)
        : _name(name) {
        _type_index = type_index;
    }
    TypeInfo(luisa::string_view name, uint8_t const *ptr)
        : _name(name) {
        std::memcpy(&_type_index, ptr, 16);
    }
    [[nodiscard]] MD5 md5() const { return _type_index; }
    [[nodiscard]] RBC_CORE_API luisa::string md5_to_string(bool upper = false) const;
    [[nodiscard]] RBC_CORE_API luisa::string md5_to_base64() const;

    [[nodiscard]] auto hash() const { return _type_index.to_binary().data0; }
    [[nodiscard]] auto name() const { return _name; }
    // This is OK, because address must come from const char*
    [[nodiscard]] auto name_c_str() const { return _name.data(); }

    template<concepts::RTTIType T>
    static TypeInfo get() {
        using TypeMeta = rbc_rtti_detail::is_rtti_type<T>;
        if constexpr (requires { TypeMeta::md5; }) {
            return TypeInfo{TypeMeta::name, TypeMeta::md5};
        } else if constexpr (requires { TypeMeta::get_md5(); }) {
            auto bin = TypeMeta::get_md5();
            return TypeInfo{TypeMeta::name, bin};
        } else {
            static_assert(luisa::always_false_v<T>, "Type MD5 is missing, must define is_rtti_type<T>::md5 or is_rtti_type<T>::get_md5()");
        }
    }

    static TypeInfo invalid() {
        return TypeInfo{"invalid type", MD5{vstd::MD5::MD5Data{~0ull, ~0ull}}};
    }

    bool operator==(TypeInfo const &t) const {
        return _type_index == t._type_index;
    }
    bool operator<(TypeInfo const &t) const {
        if (_type_index.to_binary().data0 < t._type_index.to_binary().data0)
            return true;
        return _type_index.to_binary().data1 < t._type_index.to_binary().data1;
    }
};

template<class T>
struct type_t {
    using type = T;
};

}// namespace rbc
namespace luisa {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
        return value.hash();
    }
};

}// namespace luisa
namespace std {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
        return value.hash();
    }
};
}// namespace std
namespace vstd {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
        return value.hash();
    }
};
template<>
struct compare<rbc::TypeInfo> {
    int32_t operator()(rbc::TypeInfo const &a, rbc::TypeInfo const &b) const {
        if (a == b) return 0;
        if (a < b) return -1;
        return 1;
    }
};
}// namespace vstd