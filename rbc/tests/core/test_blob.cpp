#include "test_util.h"
#include <rbc_core/blob.h>
#include <rbc_core/memory.h>
#include <cstring>

TEST_SUITE("core") {
    TEST_CASE("blob_create_basic") {
        // Test basic blob creation with copy
        const uint8_t test_data[] = {1, 2, 3, 4, 5};
        const uint64_t test_size = sizeof(test_data);
        
        auto blob = rbc::IBlob::Create(test_data, test_size, false, "test_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == test_size);
        CHECK(blob->get_data() != nullptr);
        CHECK(blob->get_data() != test_data); // Should be a copy, not the same pointer
        
        // Verify data content
        CHECK(std::memcmp(blob->get_data(), test_data, test_size) == 0);
    }

    TEST_CASE("blob_create_empty") {
        // Test creating an empty blob
        auto blob = rbc::IBlob::Create(nullptr, 0, false, "empty_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == 0);
        CHECK(blob->get_data() == nullptr);
    }

    TEST_CASE("blob_create_move") {
        // Test blob creation with move semantics
        uint8_t *test_data = (uint8_t *)rbc_malloc(10);
        for (int i = 0; i < 10; i++) {
            test_data[i] = i;
        }
        
        auto blob = rbc::IBlob::Create(test_data, 10, true, "moved_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == 10);
        CHECK(blob->get_data() == test_data); // Should be the same pointer when moved
        
        // Verify data content
        for (int i = 0; i < 10; i++) {
            CHECK(blob->get_data()[i] == i);
        }
        
        // Cleanup is handled by blob destructor
    }

    TEST_CASE("blob_create_aligned") {
        // Test aligned blob creation
        const uint8_t test_data[] = {10, 20, 30, 40, 50, 60, 70, 80};
        const uint64_t test_size = sizeof(test_data);
        const uint64_t alignment = 16;
        
        auto blob = rbc::IBlob::CreateAligned(test_data, test_size, alignment, false, "aligned_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == test_size);
        CHECK(blob->get_data() != nullptr);
        
        // Check alignment
        uintptr_t ptr = reinterpret_cast<uintptr_t>(blob->get_data());
        CHECK((ptr % alignment) == 0);
        
        // Verify data content
        CHECK(std::memcmp(blob->get_data(), test_data, test_size) == 0);
    }

    TEST_CASE("blob_create_aligned_large") {
        // Test aligned blob with larger alignment
        const uint64_t test_size = 100;
        const uint64_t alignment = 64;
        
        uint8_t *test_data = (uint8_t *)rbc_malloc(test_size);
        for (uint64_t i = 0; i < test_size; i++) {
            test_data[i] = static_cast<uint8_t>(i % 256);
        }
        
        auto blob = rbc::IBlob::CreateAligned(test_data, test_size, alignment, false, "large_aligned_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == test_size);
        CHECK(blob->get_data() != nullptr);
        
        // Check alignment
        uintptr_t ptr = reinterpret_cast<uintptr_t>(blob->get_data());
        CHECK((ptr % alignment) == 0);
        
        // Verify data content
        CHECK(std::memcmp(blob->get_data(), test_data, test_size) == 0);
        
        rbc_free(test_data);
    }

    TEST_CASE("blob_create_aligned_move") {
        // Test aligned blob creation with move
        const uint64_t test_size = 32;
        const uint64_t alignment = 32;
        
        uint8_t *test_data = (uint8_t *)rbc_malloc_aligned(test_size, alignment);
        for (uint64_t i = 0; i < test_size; i++) {
            test_data[i] = static_cast<uint8_t>(i + 100);
        }
        
        auto blob = rbc::IBlob::CreateAligned(test_data, test_size, alignment, true, "aligned_moved_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == test_size);
        CHECK(blob->get_data() == test_data); // Should be the same pointer when moved
        
        // Check alignment
        uintptr_t ptr = reinterpret_cast<uintptr_t>(blob->get_data());
        CHECK((ptr % alignment) == 0);
        
        // Verify data content
        for (uint64_t i = 0; i < test_size; i++) {
            CHECK(blob->get_data()[i] == static_cast<uint8_t>(i + 100));
        }
    }

    TEST_CASE("blob_multiple_references") {
        // Test blob with multiple RC references
        const uint8_t test_data[] = {42, 43, 44};
        auto blob1 = rbc::IBlob::Create(test_data, sizeof(test_data), false, "shared_blob");
        CHECK(blob1.ref_count() == 1);
        
        auto blob2 = blob1;
        CHECK(blob1.ref_count() == 2);
        CHECK(blob2.ref_count() == 2);
        CHECK(blob1->get_data() == blob2->get_data()); // Should point to same data
        
        blob1.reset();
        CHECK(blob2.ref_count() == 1);
        CHECK(blob2->get_size() == sizeof(test_data));
    }

    TEST_CASE("blob_large_data") {
        // Test blob with large data
        const uint64_t large_size = 1024 * 1024; // 1MB
        uint8_t *test_data = (uint8_t *)rbc_malloc(large_size);
        for (uint64_t i = 0; i < large_size; i++) {
            test_data[i] = static_cast<uint8_t>(i % 256);
        }
        
        auto blob = rbc::IBlob::Create(test_data, large_size, false, "large_blob");
        CHECK(!blob.is_empty());
        CHECK(blob->get_size() == large_size);
        CHECK(std::memcmp(blob->get_data(), test_data, large_size) == 0);
        
        rbc_free(test_data);
    }
}

