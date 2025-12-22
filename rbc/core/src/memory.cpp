#include <rbc_core/memory.h>

#ifdef RBC_RUNTIME_USE_MIMALLOC
#include "mimalloc.h"
#endif

RBC_FORCEINLINE static void *calloc_aligned(size_t count, size_t size, size_t alignment) {
#if !defined(_WIN32)
    void *ptr = (alignment == 1) ? malloc(size * count) : NULL;
    if (!ptr) {
        alignment = (alignment > 16) ? alignment : 16;
        ptr = aligned_alloc(alignment, size * count);
    }
    if (!ptr) {
        posix_memalign(&ptr, alignment, size * count);
    }
#else
    void *ptr = _aligned_malloc(size * count, alignment);
#endif
    memset(ptr, 0, size * count);
    return ptr;
}

static const char *kDefaultOSAllocPoolName = "rbc::os_alloc";

RBC_CORE_API void *traced_os_malloc(size_t size, const char *pool_name) {
    void *ptr = malloc(size);
    RBCCAllocN(ptr, size, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
}

RBC_CORE_API void *traced_os_calloc(size_t count, size_t size, const char *pool_name) {
    void *ptr = calloc(count, size);
    RBCCAllocN(ptr, size, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
}

RBC_CORE_API void *traced_os_calloc_aligned(size_t count, size_t size, size_t alignment, const char *pool_name) {
    void *ptr = calloc_aligned(count, size, alignment);
    RBCCAllocN(ptr, size, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
}

RBC_CORE_API void *traced_os_malloc_aligned(size_t size, size_t alignment, const char *pool_name) {
#if !defined(_WIN32)
    void *ptr = (alignment == 1) ? malloc(size) : NULL;
    if (!ptr) {
        alignment = (alignment > 16) ? alignment : 16;
        ptr = aligned_alloc(alignment, size);
    }
    if (!ptr) {
        posix_memalign(&ptr, alignment, size);
    }
#else
    void *ptr = _aligned_malloc(size, alignment);
#endif
    RBCCAllocN(ptr, size, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
}

RBC_CORE_API void traced_os_free(void *p, const char *pool_name) RBC_NOEXCEPT {
    free(p);
    RBCCFreeN(p, pool_name ? pool_name : kDefaultOSAllocPoolName);
}

#if !defined(_WIN32)
#define os_free_aligned(p, alignment) free((p))
#else
#define os_free_aligned(p, alignment) _aligned_free((p))
#endif

RBC_CORE_API void traced_os_free_aligned(void *p, size_t alignment, const char *pool_name) {
    os_free_aligned(p, alignment);
    RBCCFreeN(p, pool_name ? pool_name : kDefaultOSAllocPoolName);
}

RBC_CORE_API void *traced_os_realloc(void *p, size_t newsize, const char *pool_name) {
    RBCCFreeN(p, pool_name ? pool_name : kDefaultOSAllocPoolName);
    void *ptr = realloc(p, newsize);
    RBCCAllocN(ptr, newsize, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
}

RBC_EXTERN_C RBC_CORE_API void *traced_os_realloc_aligned(void *p, size_t newsize, size_t alignment, const char *pool_name) {
#if defined(_WIN32)
    RBCCFreeN(p, pool_name ? pool_name : kDefaultOSAllocPoolName);
    void *ptr = _aligned_realloc(p, newsize, alignment);
    RBCCAllocN(ptr, newsize, pool_name ? pool_name : kDefaultOSAllocPoolName);
    return ptr;
#endif
    // There is no posix_memalign_realloc or something like that on posix. But usually realloc will
    // allocated with enough alignment for most `align` values we will use. So we try to realloc first
    // plainly. If the returned address is not aligned adequately, we will revert to
    // posix_memalign'ing a new block, copying old data, deallocating old memory.
    alignment = alignment < _Alignof(void *) ? _Alignof(void *) : alignment;
    void *new_allocation = traced_os_realloc(p, newsize, pool_name);
    if ((uint64_t)(new_allocation) % alignment != 0) {
        //log_warn("This is slow. realloc did not allocate with an alignment of %lu, requires double copy",
        //            (long unsigned int)align);
        void *new_new = traced_os_malloc_aligned(newsize, alignment, pool_name);
        memcpy(new_new, new_allocation, newsize);
        traced_os_free(new_allocation, pool_name);
        new_allocation = new_new;
    }
    return new_allocation;
}

// internal_rbc_alloc

#if defined(RBC_RUNTIME_USE_MIMALLOC)
RBC_CORE_API void *internal_rbc_malloc(size_t size, const char *pool_name) {
    void *p = mi_malloc(size);
    if (pool_name) {
        RBCCAllocN(p, size, pool_name);
    } else {
        RBCCAlloc(p, size);
    }
    return p;
}

RBC_CORE_API void *internal_rbc_calloc(size_t count, size_t size, const char *pool_name) {
    void *p = mi_calloc(count, size);
    if (pool_name) {
        RBCCAllocN(p, size, pool_name);
    } else {
        RBCCAlloc(p, size);
    }
    return p;
}

RBC_CORE_API void *internal_rbc_calloc_aligned(size_t count, size_t size, size_t alignment, const char *pool_name) {
    void *p = mi_calloc_aligned(count, size, alignment);
    if (pool_name) {
        RBCCAllocN(p, size, pool_name);
    } else {
        RBCCAlloc(p, size);
    }
    return p;
}

RBC_CORE_API void *internal_rbc_malloc_aligned(size_t size, size_t alignment, const char *pool_name) {
    void *p = mi_malloc_aligned(size, alignment);
    if (pool_name) {
        RBCCAllocN(p, size, pool_name);
    } else {
        RBCCAlloc(p, size);
    }
    return p;
}

RBC_EXTERN_C RBC_CORE_API void *internal_rbc_new_n(size_t count, size_t size, const char *pool_name) {
    void *p = mi_new_n(count, size);
    if (pool_name) {
        RBCCAllocN(p, size * count, pool_name);
    } else {
        RBCCAlloc(p, size * count);
    }
    return p;
}

RBC_CORE_API void *internal_rbc_new_aligned(size_t size, size_t alignment, const char *pool_name) {
    void *p = mi_new_aligned(size, alignment);
    if (pool_name) {
        RBCCAllocN(p, size, pool_name);
    } else {
        RBCCAlloc(p, size);
    }
    return p;
}

RBC_CORE_API void internal_rbc_free(void *p, const char *pool_name) RBC_NOEXCEPT {
    if (pool_name) {
        RBCCFreeN(p, pool_name);
    } else {
        RBCCFree(p);
    }
    mi_free(p);
}

RBC_CORE_API void internal_rbc_free_aligned(void *p, size_t alignment, const char *pool_name) {
    if (pool_name) {
        RBCCFreeN(p, pool_name);
    } else {
        RBCCFree(p);
    }
    mi_free_aligned(p, alignment);
}

RBC_CORE_API void *internal_rbc_realloc(void *p, size_t newsize, const char *pool_name) {
    if (pool_name) {
        RBCCFreeN(p, pool_name);
    } else {
        RBCCFree(p);
    }
    void *np = mi_realloc(p, newsize);
    if (pool_name) {
        RBCCAllocN(np, newsize, pool_name);
    } else {
        RBCCAlloc(np, newsize);
    }
    return np;
}

RBC_CORE_API void *internal_rbc_realloc_aligned(void *p, size_t newsize, size_t alignment, const char *pool_name) {
    if (pool_name) {
        RBCCFreeN(p, pool_name);
    } else {
        RBCCFree(p);
    }
    void *np = mi_realloc_aligned(p, newsize, alignment);
    if (pool_name) {
        RBCCAllocN(np, newsize, pool_name);
    } else {
        RBCCAlloc(np, newsize);
    }
    return np;
}

#elif RBC_ARCH_WA

#else

RBC_CORE_API void *internal_rbc_malloc(size_t size, const char *pool_name) {
    return traced_os_malloc(size, pool_name);
}

RBC_CORE_API void *internal_rbc_calloc(size_t count, size_t size, const char *pool_name) {
    return traced_os_calloc(count, size, pool_name);
}

RBC_EXTERN_C RBC_CORE_API void *internal_rbc_new_n(size_t count, size_t size, const char *pool_name) {
    void *p = malloc(count * size);
    return p;
}

RBC_CORE_API void *internal_rbc_calloc_aligned(size_t count, size_t size, size_t alignment, const char *pool_name) {
    return traced_os_calloc_aligned(count, size, alignment, pool_name);
}

RBC_CORE_API void *internal_rbc_malloc_aligned(size_t size, size_t alignment, const char *pool_name) {
    return traced_os_malloc_aligned(size, alignment, pool_name);
}

RBC_CORE_API void *internal_rbc_new_aligned(size_t size, size_t alignment, const char *pool_name) {
    return traced_os_malloc_aligned(size, alignment, pool_name);
}

RBC_CORE_API void internal_rbc_free(void *p, const char *pool_name) {
    return traced_os_free(p, pool_name);
}

RBC_CORE_API void internal_rbc_free_aligned(void *p, size_t alignment, const char *pool_name) {
    traced_os_free_aligned(p, alignment, pool_name);
}

RBC_CORE_API void *internal_rbc_realloc(void *p, size_t newsize, const char *pool_name) {
    return traced_os_realloc(p, newsize, pool_name);
}

RBC_CORE_API void *internal_rbc_realloc_aligned(void *p, size_t newsize, size_t align, const char *pool_name) {
    return traced_os_realloc_aligned(p, newsize, align, pool_name);
}

#endif

const char *kContainersDefaultPoolName = "rbc::containers";

void *containers_malloc_aligned(size_t size, size_t alignment) {
#if defined(TRACY_TRACE_ALLOCATION)
    RBCCZoneNCS(z, "containers::allocate", RBC_ALLOC_TRACY_MARKER_COLOR, 16, 1);
    void *p = internal_rbc_malloc_aligned(size, alignment, kContainersDefaultPoolName);
    RBCCZoneEnd(z);
    return p;
#else
    return rbc_malloc_aligned(size, alignment);
#endif
}

void containers_free_aligned(void *p, size_t alignment) {
#if defined(TRACY_TRACE_ALLOCATION)
    RBCCZoneNCS(z, "containers::free", RBC_DEALLOC_TRACY_MARKER_COLOR, 16, 1);
    internal_rbc_free_aligned(p, alignment, kContainersDefaultPoolName);
    RBCCZoneEnd(z);
#else
    rbc_free_aligned(p, alignment);
#endif
}
