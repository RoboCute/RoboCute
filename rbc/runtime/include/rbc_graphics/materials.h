#pragma once

#include <array>
#include <luisa/vstl/common.h>

namespace rbc {
struct MatVolumeHandle {
    uint index{~0u};
};
struct MatImageHandle {
    uint uv_type_heap_index{~0u};
    static constexpr uint mask = (1u << 30u) - 1u;
    constexpr MatImageHandle() {}
    constexpr MatImageHandle(uint uv, uint handle) noexcept : uv_type_heap_index((uv << 30u) | (handle & ((1u << 30u) - 1))) {}
    uint type() const noexcept {
        return uv_type_heap_index >> 30u;
    }
    uint index() const noexcept {
        return uv_type_heap_index & mask;
    }
    void set_type(uint type) {
        uv_type_heap_index = (uv_type_heap_index & mask) | (type << 30u);
    }
    void set_index(uint index) {
        uv_type_heap_index = (index & mask) | (uv_type_heap_index & (~mask));
    }
};
struct MatBufferHandle {
    uint index{~0u};
};
}// namespace rbc