#pragma once
#include <array>
#include <luisa/vstl/v_guid.h>

namespace luisa {
template<>
struct hash<std::array<uint64_t, 2>> {
    using T = std::array<uint64_t, 2>;
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(T const &value, uint64_t seed = hash64_default_seed) const noexcept {
        return luisa::hash64(value.data(), value.size() * sizeof(uint64_t), seed);
    }
};
template<>
struct hash<vstd::Guid> {
    using T = vstd::Guid;
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(T const &value, uint64_t seed = hash64_default_seed) const noexcept {
        return luisa::hash64(&value, sizeof(T), seed);
    }
};
}// namespace luisa

namespace std {
template<>
struct equal_to<std::array<uint64_t, 2>> {
    using T = std::array<uint64_t, 2>;
    [[nodiscard]] bool operator()(T const &a, T const &b) const noexcept {
        return a[0] == b[0] && a[1] == b[1];
    }
};
}// namespace std
