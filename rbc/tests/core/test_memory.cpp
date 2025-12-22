#include "test_util.h"
#include <rbc_core/memory.h>
#include <cstring>
#include <cstdint>

TEST_SUITE("core") {
    TEST_CASE("memory_malloc_basic") {
        // Test basic malloc
        void *ptr = rbc_malloc(100);
        CHECK(ptr != nullptr);
        rbc_free(ptr);
    }

    TEST_CASE("memory_malloc_zero_size") {
        // Test malloc with zero size (implementation defined behavior)
        void *ptr = rbc_malloc(0);
        // Some allocators return nullptr, some return a valid pointer
        // Both are acceptable, just check it doesn't crash
        if (ptr != nullptr) {
            rbc_free(ptr);
        }
    }

    TEST_CASE("memory_calloc_basic") {
        // Test calloc - should zero-initialize memory
        const size_t count = 10;
        const size_t size = sizeof(int);
        int *ptr = (int *)rbc_calloc(count, size);
        CHECK(ptr != nullptr);
        
        // Verify zero-initialization
        for (size_t i = 0; i < count; i++) {
            CHECK(ptr[i] == 0);
        }
        
        rbc_free(ptr);
    }

    TEST_CASE("memory_malloc_aligned") {
        // Test aligned malloc
        const size_t size = 100;
        const size_t alignment = 16;
        
        void *ptr = rbc_malloc_aligned(size, alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        rbc_free_aligned(ptr, alignment);
    }

    TEST_CASE("memory_malloc_aligned_various") {
        // Test various alignment values
        const size_t size = 100;
        size_t alignments[] = {1, 4, 8, 16, 32, 64, 128};
        
        for (size_t alignment : alignments) {
            void *ptr = rbc_malloc_aligned(size, alignment);
            CHECK(ptr != nullptr);
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            CHECK((addr % alignment) == 0);
            
            rbc_free_aligned(ptr, alignment);
        }
    }

    TEST_CASE("memory_calloc_aligned") {
        // Test aligned calloc - should zero-initialize
        const size_t count = 10;
        const size_t size = sizeof(int);
        const size_t alignment = 16;
        
        int *ptr = (int *)rbc_calloc_aligned(count, size, alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        // Verify zero-initialization
        for (size_t i = 0; i < count; i++) {
            CHECK(ptr[i] == 0);
        }
        
        rbc_free_aligned(ptr, alignment);
    }

    TEST_CASE("memory_realloc") {
        // Test realloc
        void *ptr = rbc_malloc(10);
        CHECK(ptr != nullptr);
        
        // Write some data
        std::memset(ptr, 42, 10);
        
        // Reallocate to larger size
        void *new_ptr = rbc_realloc(ptr, 100);
        CHECK(new_ptr != nullptr);
        
        // Verify old data is preserved (first 10 bytes)
        uint8_t *bytes = (uint8_t *)new_ptr;
        for (size_t i = 0; i < 10; i++) {
            CHECK(bytes[i] == 42);
        }
        
        rbc_free(new_ptr);
    }

    TEST_CASE("memory_realloc_shrink") {
        // Test realloc to smaller size
        void *ptr = rbc_malloc(100);
        CHECK(ptr != nullptr);
        
        std::memset(ptr, 99, 100);
        
        void *new_ptr = rbc_realloc(ptr, 50);
        CHECK(new_ptr != nullptr);
        
        rbc_free(new_ptr);
    }

    TEST_CASE("memory_realloc_aligned") {
        // Test aligned realloc
        const size_t alignment = 16;
        void *ptr = rbc_malloc_aligned(10, alignment);
        CHECK(ptr != nullptr);
        
        std::memset(ptr, 77, 10);
        
        void *new_ptr = rbc_realloc_aligned(ptr, 100, alignment);
        CHECK(new_ptr != nullptr);
        
        // Check alignment is maintained
        uintptr_t addr = reinterpret_cast<uintptr_t>(new_ptr);
        CHECK((addr % alignment) == 0);
        
        // Verify old data is preserved
        uint8_t *bytes = (uint8_t *)new_ptr;
        for (size_t i = 0; i < 10; i++) {
            CHECK(bytes[i] == 77);
        }
        
        rbc_free_aligned(new_ptr, alignment);
    }

    TEST_CASE("memory_new_n") {
        // Test new_n (array allocation)
        const size_t count = 5;
        const size_t size = sizeof(int);
        
        int *ptr = (int *)rbc_new_n(count, size);
        CHECK(ptr != nullptr);
        
        // Initialize and verify
        for (size_t i = 0; i < count; i++) {
            ptr[i] = static_cast<int>(i * 10);
        }
        
        for (size_t i = 0; i < count; i++) {
            CHECK(ptr[i] == static_cast<int>(i * 10));
        }
        
        rbc_free(ptr);
    }

    TEST_CASE("memory_new_aligned") {
        // Test aligned new
        const size_t size = sizeof(int);
        const size_t alignment = 16;
        
        int *ptr = (int *)rbc_new_aligned(size, alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        *ptr = 12345;
        CHECK(*ptr == 12345);
        
        rbc_free_aligned(ptr, alignment);
    }

    TEST_CASE("memory_pool_name") {
        // Test allocation with pool name
        const char *pool_name = "test_pool";
        void *ptr = rbc_mallocN(100, pool_name);
        CHECK(ptr != nullptr);
        rbc_freeN(ptr, pool_name);
    }

    TEST_CASE("memory_multiple_allocations") {
        // Test multiple allocations and deallocations
        const int num_allocs = 100;
        void *ptrs[num_allocs];
        
        // Allocate
        for (int i = 0; i < num_allocs; i++) {
            ptrs[i] = rbc_malloc(10);
            CHECK(ptrs[i] != nullptr);
        }
        
        // Deallocate
        for (int i = 0; i < num_allocs; i++) {
            rbc_free(ptrs[i]);
        }
    }

    TEST_CASE("memory_write_read") {
        // Test writing and reading from allocated memory
        const size_t size = 1000;
        uint8_t *ptr = (uint8_t *)rbc_malloc(size);
        CHECK(ptr != nullptr);
        
        // Write pattern
        for (size_t i = 0; i < size; i++) {
            ptr[i] = static_cast<uint8_t>(i % 256);
        }
        
        // Read and verify
        for (size_t i = 0; i < size; i++) {
            CHECK(ptr[i] == static_cast<uint8_t>(i % 256));
        }
        
        rbc_free(ptr);
    }

    TEST_CASE("memory_aligned_write_read") {
        // Test writing and reading from aligned memory
        const size_t size = 1000;
        const size_t alignment = 64;
        
        uint8_t *ptr = (uint8_t *)rbc_malloc_aligned(size, alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        // Write pattern
        for (size_t i = 0; i < size; i++) {
            ptr[i] = static_cast<uint8_t>((i * 7) % 256);
        }
        
        // Read and verify
        for (size_t i = 0; i < size; i++) {
            CHECK(ptr[i] == static_cast<uint8_t>((i * 7) % 256));
        }
        
        rbc_free_aligned(ptr, alignment);
    }

    TEST_CASE("memory_containers_functions") {
        // Test containers_malloc_aligned and containers_free_aligned
        const size_t size = 100;
        const size_t alignment = 16;
        
        void *ptr = containers_malloc_aligned(size, alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        containers_free_aligned(ptr, alignment);
    }
}

