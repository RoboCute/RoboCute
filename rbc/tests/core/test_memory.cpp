#include "test_util.h"
#include <rbc_core/memory.h>
#include <cstring>
#include <cstdint>

    "memory_malloc_basic"_test = [] {
        // Test basic malloc
        void *ptr = rbc_malloc(100);
        expect(static_cast<bool>(ptr != nullptr));
        rbc_free(ptr);
    };

    "memory_malloc_zero_size"_test = [] {
        // Test malloc with zero size (implementation defined behavior)
        void *ptr = rbc_malloc(0);
        // Some allocators return nullptr, some return a valid pointer
        // Both are acceptable, just check it doesn't crash
        if (ptr != nullptr) {
            rbc_free(ptr);
        }
    };

    "memory_calloc_basic"_test = [] {
        // Test calloc - should zero-initialize memory
        const size_t count = 10;
        const size_t size = sizeof(int);
        int *ptr = (int *)rbc_calloc(count, size);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Verify zero-initialization
        for (size_t i = 0; i < count; i++) {
            expect(static_cast<bool>(ptr[i] == 0));
        }
        
        rbc_free(ptr);
    };

    "memory_malloc_aligned"_test = [] {
        // Test aligned malloc
        const size_t size = 100;
        const size_t alignment = 16;
        
        void *ptr = rbc_malloc_aligned(size, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        rbc_free_aligned(ptr, alignment);
    };

    "memory_malloc_aligned_various"_test = [] {
        // Test various alignment values
        const size_t size = 100;
        size_t alignments[] = {1, 4, 8, 16, 32, 64, 128};
        
        for (size_t alignment : alignments) {
            void *ptr = rbc_malloc_aligned(size, alignment);
            expect(static_cast<bool>(ptr != nullptr));
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            expect(static_cast<bool>((addr % alignment) == 0));
            
            rbc_free_aligned(ptr, alignment);
        }
    };

    "memory_calloc_aligned"_test = [] {
        // Test aligned calloc - should zero-initialize
        const size_t count = 10;
        const size_t size = sizeof(int);
        const size_t alignment = 16;
        
        int *ptr = (int *)rbc_calloc_aligned(count, size, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        // Verify zero-initialization
        for (size_t i = 0; i < count; i++) {
            expect(static_cast<bool>(ptr[i] == 0));
        }
        
        rbc_free_aligned(ptr, alignment);
    };

    "memory_realloc"_test = [] {
        // Test realloc
        void *ptr = rbc_malloc(10);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Write some data
        std::memset(ptr, 42, 10);
        
        // Reallocate to larger size
        void *new_ptr = rbc_realloc(ptr, 100);
        expect(static_cast<bool>(new_ptr != nullptr));
        
        // Verify old data is preserved (first 10 bytes)
        uint8_t *bytes = (uint8_t *)new_ptr;
        for (size_t i = 0; i < 10; i++) {
            expect(static_cast<bool>(bytes[i] == 42));
        }
        
        rbc_free(new_ptr);
    };

    "memory_realloc_shrink"_test = [] {
        // Test realloc to smaller size
        void *ptr = rbc_malloc(100);
        expect(static_cast<bool>(ptr != nullptr));
        
        std::memset(ptr, 99, 100);
        
        void *new_ptr = rbc_realloc(ptr, 50);
        expect(static_cast<bool>(new_ptr != nullptr));
        
        rbc_free(new_ptr);
    };

    "memory_realloc_aligned"_test = [] {
        // Test aligned realloc
        const size_t alignment = 16;
        void *ptr = rbc_malloc_aligned(10, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        std::memset(ptr, 77, 10);
        
        void *new_ptr = rbc_realloc_aligned(ptr, 100, alignment);
        expect(static_cast<bool>(new_ptr != nullptr));
        
        // Check alignment is maintained
        uintptr_t addr = reinterpret_cast<uintptr_t>(new_ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        // Verify old data is preserved
        uint8_t *bytes = (uint8_t *)new_ptr;
        for (size_t i = 0; i < 10; i++) {
            expect(static_cast<bool>(bytes[i] == 77));
        }
        
        rbc_free_aligned(new_ptr, alignment);
    };

    "memory_new_n"_test = [] {
        // Test new_n (array allocation)
        const size_t count = 5;
        const size_t size = sizeof(int);
        
        int *ptr = (int *)rbc_new_n(count, size);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Initialize and verify
        for (size_t i = 0; i < count; i++) {
            ptr[i] = static_cast<int>(i * 10);
        }
        
        for (size_t i = 0; i < count; i++) {
            expect(static_cast<bool>(ptr[i] == static_cast<int>(i * 10)));
        }
        
        rbc_free(ptr);
    };

    "memory_new_aligned"_test = [] {
        // Test aligned new
        const size_t size = sizeof(int);
        const size_t alignment = 16;
        
        int *ptr = (int *)rbc_new_aligned(size, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        *ptr = 12345;
        expect(static_cast<bool>(*ptr == 12345));
        
        rbc_free_aligned(ptr, alignment);
    };

    "memory_pool_name"_test = [] {
        // Test allocation with pool name
        const char *pool_name = "test_pool";
        void *ptr = rbc_mallocN(100, pool_name);
        expect(static_cast<bool>(ptr != nullptr));
        rbc_freeN(ptr, pool_name);
    };

    "memory_multiple_allocations"_test = [] {
        // Test multiple allocations and deallocations
        const int num_allocs = 100;
        void *ptrs[num_allocs];
        
        // Allocate
        for (int i = 0; i < num_allocs; i++) {
            ptrs[i] = rbc_malloc(10);
            expect(static_cast<bool>(ptrs[i] != nullptr));
        }
        
        // Deallocate
        for (int i = 0; i < num_allocs; i++) {
            rbc_free(ptrs[i]);
        }
    };

    "memory_write_read"_test = [] {
        // Test writing and reading from allocated memory
        const size_t size = 1000;
        uint8_t *ptr = (uint8_t *)rbc_malloc(size);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Write pattern
        for (size_t i = 0; i < size; i++) {
            ptr[i] = static_cast<uint8_t>(i % 256);
        }
        
        // Read and verify
        for (size_t i = 0; i < size; i++) {
            expect(static_cast<bool>(ptr[i] == static_cast<uint8_t>(i % 256)));
        }
        
        rbc_free(ptr);
    };

    "memory_aligned_write_read"_test = [] {
        // Test writing and reading from aligned memory
        const size_t size = 1000;
        const size_t alignment = 64;
        
        uint8_t *ptr = (uint8_t *)rbc_malloc_aligned(size, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        // Write pattern
        for (size_t i = 0; i < size; i++) {
            ptr[i] = static_cast<uint8_t>((i * 7) % 256);
        }
        
        // Read and verify
        for (size_t i = 0; i < size; i++) {
            expect(static_cast<bool>(ptr[i] == static_cast<uint8_t>((i * 7) % 256)));
        }
        
        rbc_free_aligned(ptr, alignment);
    };

    "memory_containers_functions"_test = [] {
        // Test containers_malloc_aligned and containers_free_aligned
        const size_t size = 100;
        const size_t alignment = 16;
        
        void *ptr = containers_malloc_aligned(size, alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        containers_free_aligned(ptr, alignment);
    };
