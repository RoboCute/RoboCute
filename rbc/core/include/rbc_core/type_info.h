#pragma once
#include <rbc_config.h>
#include <luisa/core//stl/string.h>
#include <luisa/core//stl/hash.h>
#include <luisa/vstl/hash.h>
#include <luisa/vstl/compare.h>
#include <type_traits>
#include <cstdint>
#include <rbc_core/rtti.h>
namespace rbc {
namespace concepts {
template<typename T>
concept RTTIType = is_rtti_type_v<T>;
}// namespace concepts
struct TypeInfo {
private:
    luisa::string_view _name;
    uint64_t _md5[2];
    TypeInfo(luisa::string_view name, uint64_t d0, uint64_t d1)
        : _name(name) {
        _md5[0] = d0;
        _md5[1] = d1;
    }
    TypeInfo(luisa::string_view name, uint8_t const *ptr)
        : _name(name) {
        std::memcpy(_md5, ptr, 16);
    }
public:
    [[nodiscard]] std::array<uint64_t, 2> md5() const { return {_md5[0], _md5[1]}; }
    [[nodiscard]] RBC_CORE_API luisa::string md5_to_string(bool upper = false) const;
    [[nodiscard]] RBC_CORE_API luisa::string md5_to_base64() const;

    [[nodiscard]] auto hash() const { return _md5[0]; }
    [[nodiscard]] auto name() const { return _name; }

    template<concepts::RTTIType T>
    static TypeInfo get() {
        using TypeMeta = rbc_rtti_detail::is_rtti_type<T>;
        if constexpr (requires { TypeMeta::md5; }) {
            return TypeInfo{TypeMeta::name, TypeMeta::md5};
        } else if constexpr (requires { TypeMeta::get_md5(); }) {
            auto bin = TypeMeta::get_md5();
            return TypeInfo{TypeMeta::name, bin[0], bin[1]};
        } else {
            static_assert(luisa::always_false_v<T>, "Type MD5 is missing, must define is_rtti_type<T>::md5 or is_rtti_type<T>::get_md5()");
        }
    }
    bool operator==(TypeInfo const &t) const {
        return _md5[0] == t._md5[0] && _md5[1] == t._md5[1];
    }
    bool operator<(TypeInfo const &t) const {
        if (_md5[0] < t._md5[0])
            return true;
        return _md5[1] < t._md5[1];
    }
};
}// namespace rbc
namespace luisa {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] constexpr uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
        return value.hash();
    }
};
}// namespace luisa
namespace std {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] constexpr uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
        return value.hash();
    }
};
}// namespace std
namespace vstd {
template<>
struct hash<rbc::TypeInfo> {
    using is_avalanching = void;
    [[nodiscard]] constexpr uint64_t operator()(const rbc::TypeInfo &value) const noexcept {
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