/**
 * @file se_test_util.h
 * @brief Header for sail engine test utility functions and macros
 * @author sailing-innocent
 * @date 2023-09-15
 */

#pragma once

// Approx helper to replace doctest::Approx
template <class T>
struct Approx {
    T value;
    double epsilon = 1e-6;
    constexpr explicit Approx(T v) : value(v) {}
};

template <class T, class U>
constexpr bool operator==(const T &actual, const Approx<U> &expected) {
    using std::abs;
    return abs(static_cast<double>(actual) - static_cast<double>(expected.value)) <= expected.epsilon;
}

template <class T, class U>
constexpr bool operator!=(const T &actual, const Approx<U> &expected) {
    return !(actual == expected);
}

#include "ut/ut.hpp"
using namespace boost::ut;
#include <span>
#include <concepts>

namespace sail::test {

[[nodiscard]] int argc() noexcept;
[[nodiscard]] const char *const *argv() noexcept;
[[nodiscard]] bool float_span_equal(std::span<float> a, std::span<float> b);

// concept for normal value types
template<typename T>
concept CommonValueType = std::is_arithmetic_v<T>;

}// namespace sail::test