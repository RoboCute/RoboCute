#pragma once

#include <doctest.h>
#include <span>
#include <concepts>
#include <luisa/vstl/common.h>

namespace rbc::test {

[[nodiscard]] int argc() noexcept;
[[nodiscard]] const char *const *argv() noexcept;
template<std::size_t N>
[[nodiscard]] inline bool feq(const luisa::Vector<float, N> &a, const luisa::Vector<float, N> &b) noexcept {
    bool res = true;
    for (std::size_t i = 0; i < N; ++i) {
        res = res && a[i] == doctest::Approx(b[i]);
        CHECK(a[i] == doctest::Approx(b[i]));
    }
    return res;
}

}// namespace rbc::test
