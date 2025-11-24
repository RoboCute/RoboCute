#include <rbc_core/heap_object.h>
#include <luisa/core/logging.h>
namespace rbc {
RBC_CORE_API void *HeapObjectMeta::allocate() const {
    LUISA_DEBUG_ASSERT(default_ctor || is_trivial_constructible);
    auto ptr = luisa::detail::allocator_allocate(size, alignment);
    if (is_trivial_constructible) {
        std::memset(ptr, 0, size);
    } else {
        default_ctor(ptr);
    }
    return ptr;
}
RBC_CORE_API void HeapObjectMeta::deallocate(void *ptr) const {
    if (deleter) {
        deleter(ptr);
    }
    luisa::detail::allocator_deallocate(ptr, alignment);
}
RBC_CORE_API void HeapObjectMeta::copy(void *dst, void *src) const {
    LUISA_DEBUG_ASSERT(copy_ctor || is_trivial_copyable);
    if (is_trivial_copyable) {
        std::memcpy(dst, src, size);
    } else {
        copy_ctor(dst, src);
    }
}
RBC_CORE_API void HeapObjectMeta::move(void *dst, void *src) const {
    LUISA_DEBUG_ASSERT(move_ctor || is_trivial_movable);
    if (is_trivial_movable) {
        std::memcpy(dst, src, size);
    } else {
        move_ctor(dst, src);
    }
}
}// namespace rbc