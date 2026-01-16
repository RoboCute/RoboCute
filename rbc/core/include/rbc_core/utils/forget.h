#pragma once
#include <type_traits>
#include <cstddef>
namespace rbc {
template<typename T>
    requires(!std::is_reference_v<T>) && (std::is_move_constructible_v<T>)
void unsafe_forget(T &&value) {
    alignas(T) std::byte data[sizeof(T)];
    new (data) T(std::forward<T>(value));
}
}// namespace rbc