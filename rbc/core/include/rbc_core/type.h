#pragma once
#include <rbc_config.h>
#include <luisa/core//stl/string.h>
#include <luisa/core//stl/hash.h>
#include <luisa/vstl/hash.h>
#include <luisa/vstl/compare.h>
#include <type_traits>
#include <cstdint>
namespace rbc {
namespace concepts {
template<typename T>
concept RTTIType = requires() {
    std::is_same_v<decltype(T::_zz_typename_), luisa::string_view>;
    std::is_same_v<decltype(T::_zz_md5_), uint64_t[2]>;
};
}// namespace concepts
struct TypeInfo {
private:
    luisa::string_view _name;
    uint64_t _md5[2];
    TypeInfo(luisa::string_view name, uint64_t const md5[2])
        : _name(name) {
        _md5[0] = md5[0];
        _md5[1] = md5[1];
    }
public:
    [[nodiscard]] std::array<uint64_t, 2> md5() const { return {_md5[0], _md5[1]}; }
    [[nodiscard]] RBC_CORE_API luisa::string md5_to_string(bool upper = false) const;

    [[nodiscard]] auto hash() const { return _md5[0]; }
    [[nodiscard]] auto name() const { return _name; }

    template<concepts::RTTIType T>
    static TypeInfo get() {
        return TypeInfo{T::_zz_typename_, T::_zz_md5_};
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