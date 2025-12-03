#pragma once
#include <atomic>
#include <algorithm>
#include <thread>
#include <luisa/core/intrin.h>
namespace rbc {
template<typename T>
void atomic_min(std::atomic<T> &atomic_val, T new_val) {
    T old_val = atomic_val.load(std::memory_order_relaxed);
    while (new_val < old_val &&
           !atomic_val.compare_exchange_weak(
               old_val, new_val,
               std::memory_order_release,
               std::memory_order_relaxed)) {
        LUISA_INTRIN_PAUSE();
    }
}
template<typename T>
void atomic_max(std::atomic<T> &atomic_val, T new_val) {
    T old_val = atomic_val.load(std::memory_order_relaxed);
    while (new_val > old_val &&
           !atomic_val.compare_exchange_weak(
               old_val, new_val,
               std::memory_order_release,
               std::memory_order_relaxed)) {
        LUISA_INTRIN_PAUSE();
    }
}
}// namespace rbc