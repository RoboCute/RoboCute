#pragma once
#include <rbc_config.h>
#include <tracy_wrapper.h>
#include <new>
#include <type_traits>

//=======================basic alloc=======================
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_malloc(size_t size, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_calloc(size_t count, size_t size, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_malloc_aligned(size_t size, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_calloc_aligned(size_t count, size_t size, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_new_n(size_t count, size_t size, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_new_aligned(size_t size, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void internal_rbc_free(void *p, const char *pool_name) RBC_NOEXCEPT;
RBC_EXTERN_C RBC_CORE_API void internal_rbc_free_aligned(void *p, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_realloc(void *p, size_t newsize, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *internal_rbc_realloc_aligned(void *p, size_t newsize, size_t alignment, const char *pool_name);

RBC_EXTERN_C RBC_CORE_API void *traced_os_malloc(size_t size, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *traced_os_calloc(size_t count, size_t size, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *traced_os_malloc_aligned(size_t size, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *traced_os_calloc_aligned(size_t count, size_t size, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void traced_os_free(void *p, const char *pool_name) RBC_NOEXCEPT;
RBC_EXTERN_C RBC_CORE_API void traced_os_free_aligned(void *p, size_t alignment, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *traced_os_realloc(void *p, size_t newsize, const char *pool_name);
RBC_EXTERN_C RBC_CORE_API void *traced_os_realloc_aligned(void *p, size_t newsize, size_t alignment, const char *pool_name);

RBC_EXTERN_C RBC_CORE_API void *containers_malloc_aligned(size_t size, size_t alignment);
RBC_EXTERN_C RBC_CORE_API void containers_free_aligned(void *p, size_t alignment);

//=======================alloc with trace=======================
#if defined(RBC_PROFILE_ENABLE) && defined(TRACY_TRACE_ALLOCATION)
#define RBC_ALLOC_TRACY_MARKER_COLOR 0xff0000
#define RBC_DEALLOC_TRACY_MARKER_COLOR 0x0000ff

RBC_FORCEINLINE void *RBCMallocWithCZone(size_t size, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_malloc(size, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
RBC_FORCEINLINE void *RBCCallocWithCZone(size_t count, size_t size, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_calloc(count, size, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}

RBC_FORCEINLINE void *RBCMallocAlignedWithCZone(size_t size, size_t alignment, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_malloc_aligned(size, alignment, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}

RBC_FORCEINLINE void *RBCCallocAlignedWithCZone(size_t count, size_t size, size_t alignment, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_calloc_aligned(count, size, alignment, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
RBC_FORCEINLINE void *RBCNewNWithCZone(size_t count, size_t size, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_new_n(count, size, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
RBC_FORCEINLINE void *RBCNewAlignedWithCZone(size_t size, size_t alignment, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_new_aligned(size, alignment, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
RBC_FORCEINLINE void RBCFreeWithCZone(void *p, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_DEALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    internal_rbc_free(p, pool_name);
    RBCCZoneEnd(z);
}
RBC_FORCEINLINE void RBCFreeAlignedWithCZone(void *p, size_t alignment, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_DEALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    internal_rbc_free_aligned(p, alignment, pool_name);
    RBCCZoneEnd(z);
}
RBC_FORCEINLINE void *RBCReallocWithCZone(void *p, size_t newsize, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_realloc(p, newsize, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
RBC_FORCEINLINE void *RBCReallocAlignedWithCZone(void *p, size_t newsize, size_t alignment, const char *line, const char *pool_name) {
    RBCCZoneC(z, RBC_ALLOC_TRACY_MARKER_COLOR, 1);
    RBCCZoneText(z, line, strlen(line));
    RBCCZoneName(z, line, strlen(line));
    void *ptr = internal_rbc_realloc_aligned(p, newsize, alignment, pool_name);
    RBCCZoneEnd(z);
    return ptr;
}
#endif

//=======================alloc defines=======================
#if defined(RBC_PROFILE_ENABLE) && defined(TRACY_TRACE_ALLOCATION)
#define RBC_ALLOC_STRINGFY_IMPL(X) #X
#define RBC_ALLOC_STRINGFY(X) RBC_ALLOC_STRINGFY_IMPL(X)
#define RBC_ALLOC_CAT_IMPL(X, Y) X Y
#define RBC_ALLOC_CAT(X, Y) RBC_ALLOC_CAT_IMPL(X, Y)

#define rbc_malloc(size) RBCMallocWithCZone((size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_calloc(count, size) RBCCallocWithCZone((count), (size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_malloc_aligned(size, alignment) RBCMallocAlignedWithCZone((size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_calloc_aligned(count, size, alignment) RBCCallocAlignedWithCZone((count), (size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_new_n(count, size) RBCNewNWithCZone((count), (size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_new_aligned(size, alignment) RBCNewAlignedWithCZone((size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_realloc(p, newsize) RBCReallocWithCZone((p), (newsize), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_realloc_aligned(p, newsize, align) RBCReallocAlignedWithCZone((p), (newsize), (align), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_free(p) RBCFreeWithCZone((p), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)
#define rbc_free_aligned(p, alignment) RBCFreeAlignedWithCZone((p), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), NULL)

#define rbc_mallocN(size, ...) RBCMallocWithCZone((size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_callocN(count, size, ...) RBCCallocWithCZone((count), (size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_malloc_alignedN(size, alignment, ...) RBCMallocAlignedWithCZone((size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_calloc_alignedN(count, size, alignment, ...) RBCCallocAlignedWithCZone((count), (size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_new_nN(count, size, ...) RBCNewNWithCZone((count), (size), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_new_alignedN(size, alignment, ...) RBCNewAlignedWithCZone((size), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_realloc_alignedN(p, newsize, align, ...) RBCReallocAlignedWithCZone((p), (newsize), (align), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_reallocN(p, newsize, ...) RBCReallocWithCZone((p), (newsize), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_freeN(p, ...) RBCFreeWithCZone((p), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#define rbc_free_alignedN(p, alignment, ...) RBCFreeAlignedWithCZone((p), (alignment), RBC_ALLOC_CAT(RBC_ALLOC_STRINGFY(__FILE__), RBC_ALLOC_STRINGFY(__LINE__)), __VA_ARGS__)
#else
#define rbc_malloc(size) internal_rbc_malloc((size), NULL)
#define rbc_calloc(count, size) internal_rbc_calloc((count), (size), NULL)
#define rbc_malloc_aligned(size, alignment) internal_rbc_malloc_aligned((size), (alignment), NULL)
#define rbc_calloc_aligned(count, size, alignment) internal_rbc_calloc_aligned((count), (size), (alignment), NULL)
#define rbc_new_n(count, size) internal_rbc_new_n((count), (size), NULL)
#define rbc_new_aligned(size, alignment) internal_rbc_new_aligned((size), (alignment), NULL)
#define rbc_realloc(p, newsize) internal_rbc_realloc((p), (newsize), NULL)
#define rbc_realloc_aligned(p, newsize, align) internal_rbc_realloc_aligned((p), (newsize), (align), NULL)
#define rbc_free(p) internal_rbc_free((p), NULL)
#define rbc_free_aligned(p, alignment) internal_rbc_free_aligned((p), (alignment), NULL)

#define rbc_mallocN(size, ...) internal_rbc_malloc((size), __VA_ARGS__)
#define rbc_callocN(count, size, ...) internal_rbc_calloc((count), (size), __VA_ARGS__)
#define rbc_malloc_alignedN(size, alignment, ...) internal_rbc_malloc_aligned((size), (alignment), __VA_ARGS__)
#define rbc_calloc_alignedN(count, size, alignment, ...) internal_rbc_calloc_aligned((count), (size), (alignment), __VA_ARGS__)
#define rbc_new_nN(count, size, ...) internal_rbc_new_n((count), (size), __VA_ARGS__)
#define rbc_new_alignedN(size, alignment, ...) internal_rbc_new_aligned((size), (alignment), __VA_ARGS__)
#define rbc_reallocN(p, newsize, ...) internal_rbc_realloc((p), (newsize), __VA_ARGS__)
#define rbc_realloc_alignedN(p, newsize, align, ...) internal_rbc_realloc_aligned((p), (newsize), (align), __VA_ARGS__)
#define rbc_freeN(p, ...) internal_rbc_free((p), __VA_ARGS__)
#define rbc_free_alignedN(p, alignment, ...) internal_rbc_free_aligned((p), (alignment), __VA_ARGS__)
#endif

//=======================RBCNew and RBCDelete=======================
// Template functions for type-safe new/delete operations
// These must be defined after the macro definitions above
template<typename T, typename... Args>
RBC_FORCEINLINE T *RBCNew(Args &&...args) {
    void *ptr = rbc_new_aligned(sizeof(T), alignof(T));
    if constexpr (std::is_trivially_constructible_v<T> && sizeof...(Args) == 0) {
        // Trivially constructible, no constructor call needed
        return static_cast<T *>(ptr);
    } else {
        return new (ptr) T(std::forward<Args>(args)...);
    }
}

template<typename T>
RBC_FORCEINLINE void RBCDelete(T *ptr) RBC_NOEXCEPT {
    if (ptr != nullptr) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        rbc_free_aligned(ptr, alignof(T));
    }
}

template<typename T, typename... Args>
RBC_FORCEINLINE T *RBCNewAligned(size_t alignment, Args &&...args) {
    void *ptr = rbc_new_aligned(sizeof(T), alignment);
    if constexpr (std::is_trivially_constructible_v<T> && sizeof...(Args) == 0) {
        // Trivially constructible, no constructor call needed
        return static_cast<T *>(ptr);
    } else {
        return new (ptr) T(std::forward<Args>(args)...);
    }
}

template<typename T>
RBC_FORCEINLINE void RBCDeleteAligned(T *ptr, size_t alignment) RBC_NOEXCEPT {
    if (ptr != nullptr) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        rbc_free_aligned(ptr, alignment);
    }
}

// Versions with pool name support
template<typename T, typename... Args>
RBC_FORCEINLINE T *RBCNewN(const char *pool_name, Args &&...args) {
    void *ptr = rbc_new_alignedN(sizeof(T), alignof(T), pool_name);
    if constexpr (std::is_trivially_constructible_v<T> && sizeof...(Args) == 0) {
        // Trivially constructible, no constructor call needed
        return static_cast<T *>(ptr);
    } else {
        return new (ptr) T(std::forward<Args>(args)...);
    }
}

template<typename T>
RBC_FORCEINLINE void RBCDeleteN(T *ptr, const char *pool_name) RBC_NOEXCEPT {
    if (ptr != nullptr) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        rbc_free_alignedN(ptr, alignof(T), pool_name);
    }
}

template<typename T, typename... Args>
RBC_FORCEINLINE T *RBCNewAlignedN(size_t alignment, const char *pool_name, Args &&...args) {
    void *ptr = rbc_new_alignedN(sizeof(T), alignment, pool_name);
    if constexpr (std::is_trivially_constructible_v<T> && sizeof...(Args) == 0) {
        // Trivially constructible, no constructor call needed
        return static_cast<T *>(ptr);
    } else {
        return new (ptr) T(std::forward<Args>(args)...);
    }
}

template<typename T>
RBC_FORCEINLINE void RBCDeleteAlignedN(T *ptr, size_t alignment, const char *pool_name) RBC_NOEXCEPT {
    if (ptr != nullptr) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        rbc_free_alignedN(ptr, alignment, pool_name);
    }
}