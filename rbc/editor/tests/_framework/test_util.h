#pragma once

#include <doctest.h>
#include <span>
#include <concepts>
#include <luisa/vstl/common.h>

namespace rbc::test {

[[nodiscard]] int argc() noexcept;
[[nodiscard]] const char *const *argv() noexcept;
// concept for normal value types

inline bool feq(luisa::float2 a, luisa::float2 b) {
    bool res = true;
    for (auto i = 0; i < 2; i++) {
        res = res && a[i] == doctest::Approx(b[i]);
        CHECK(a[i] == doctest::Approx(b[i]));
    }
    return res;
}

inline bool feq(luisa::float3 a, luisa::float3 b) {
    bool res = true;
    for (auto i = 0; i < 3; i++) {
        res = res && a[i] == doctest::Approx(b[i]);
        CHECK(a[i] == doctest::Approx(b[i]));
    }
    return res;
}

inline bool feq(luisa::float4 a, luisa::float4 b) {
    bool res = true;
    for (auto i = 0; i < 4; i++) {
        res = res && a[i] == doctest::Approx(b[i]);
        CHECK(a[i] == doctest::Approx(b[i]));
    }
    return res;
}

}// namespace rbc::test
