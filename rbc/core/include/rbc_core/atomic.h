#pragma once
#include <atomic>
#include <algorithm>
#include <thread>
#include <luisa/core/intrin.h>
namespace rbc {
template<typename T>
T atomic_min(std::atomic<T> &atomic_val, T new_val) {
    T old_val = atomic_val.load(std::memory_order_relaxed);
    auto less = [&] {
        if constexpr (std::is_enum_v<T>) {
            return luisa::to_underlying(new_val) < luisa::to_underlying(old_val);
        } else {
            return new_val < old_val;
        }
    };
    while (less() &&
           !atomic_val.compare_exchange_weak(
               old_val, new_val,
               std::memory_order_release,
               std::memory_order_relaxed)) {
        LUISA_INTRIN_PAUSE();
    }
    return old_val;
}
template<typename T>
T atomic_max(std::atomic<T> &atomic_val, T new_val) {
    T old_val = atomic_val.load(std::memory_order_relaxed);
    auto greater = [&] {
        if constexpr (std::is_enum_v<T>) {
            return luisa::to_underlying(new_val) > luisa::to_underlying(old_val);
        } else {
            return new_val > old_val;
        }
    };
    while (greater() &&
           !atomic_val.compare_exchange_weak(
               old_val, new_val,
               std::memory_order_release,
               std::memory_order_relaxed)) {
        LUISA_INTRIN_PAUSE();
    }
    return old_val;
}
}// namespace rbc