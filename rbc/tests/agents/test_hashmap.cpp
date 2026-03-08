/**
 * @file test_HashMap.cpp
 * @brief Comprehensive test suite for vstd::HashMap
 * 
 * This file tests all APIs of vstd::HashMap including:
 * - Construction and destruction
 * - Insertion operations (emplace, try_emplace, force_emplace)
 * - Lookup operations (find, contains)
 * - Removal operations (remove)
 * - Capacity operations (reserve, clear, size, empty)
 * - Indexer operations (key, value, boolean conversion)
 */

#include <luisa/vstl/common.h>
#include "../_framework/test_util.h"

TEST_SUITE("HashMap") {

    // ============================================
    // Basic Construction Tests
    // ============================================

    TEST_CASE("hashmap_default_construction") {
        // Test default constructor creates an empty hash map
        vstd::HashMap<int, int> map;
        CHECK(map.size() == 0);
        CHECK(map.empty());
    }

    TEST_CASE("hashmap_with_string_keys") {
        // Test HashMap with string keys and int values
        vstd::HashMap<luisa::string, int> map;
        CHECK(map.empty());
        
        map.emplace("key1", 100);
        CHECK(map.size() == 1);
        CHECK(!map.empty());
    }

    // ============================================
    // Emplace Operations Tests
    // ============================================

    TEST_CASE("hashmap_emplace_basic") {
        // Test basic emplace operation
        vstd::HashMap<int, luisa::string> map;
        
        // Emplace key-value pairs
        map.emplace(1, "one");
        map.emplace(2, "two");
        map.emplace(3, "three");
        
        CHECK(map.size() == 3);
        
        // Verify all keys exist
        auto iter1 = map.find(1);
        CHECK(iter1);
        CHECK(iter1.key() == 1);
        CHECK(iter1.value() == "one");
        
        auto iter2 = map.find(2);
        CHECK(iter2);
        CHECK(iter2.value() == "two");
        
        auto iter3 = map.find(3);
        CHECK(iter3);
        CHECK(iter3.value() == "three");
    }

    TEST_CASE("hashmap_emplace_no_overwrite") {
        // Test that emplace does not overwrite existing values
        // (this is the behavior of vstd::HashMap emplace)
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        CHECK(map.find(1).value() == 100);
        
        // Emplace with same key should NOT overwrite
        map.emplace(1, 200);
        CHECK(map.size() == 1);
        CHECK(map.find(1).value() == 100); // Value unchanged
    }

    TEST_CASE("hashmap_try_emplace_basic") {
        // Test try_emplace - only inserts if key doesn't exist
        vstd::HashMap<luisa::string, int> map;
        
        // First try_emplace should succeed
        auto result1 = map.try_emplace("key1", 100);
        CHECK(result1.second); // Insertion happened
        CHECK(result1.first);
        CHECK(result1.first.key() == "key1");
        CHECK(result1.first.value() == 100);
        
        // Second try_emplace with same key should fail
        auto result2 = map.try_emplace("key1", 200);
        CHECK(!result2.second); // No insertion
        CHECK(result2.first);
        CHECK(result2.first.value() == 100); // Value unchanged
        
        CHECK(map.size() == 1);
    }

    TEST_CASE("hashmap_try_emplace_multiple") {
        // Test try_emplace with multiple keys
        vstd::HashMap<int, double> map;
        
        map.try_emplace(1, 1.1);
        map.try_emplace(2, 2.2);
        map.try_emplace(3, 3.3);
        
        CHECK(map.size() == 3);
        
        // Verify values
        CHECK(map.find(1).value() == doctest::Approx(1.1));
        CHECK(map.find(2).value() == doctest::Approx(2.2));
        CHECK(map.find(3).value() == doctest::Approx(3.3));
    }

    TEST_CASE("hashmap_force_emplace") {
        // Test force_emplace - always inserts/replaces
        vstd::HashMap<int, luisa::string> map;
        
        map.force_emplace(1, "first");
        CHECK(map.find(1).value() == "first");
        
        // force_emplace should overwrite existing value
        map.force_emplace(1, "second");
        CHECK(map.find(1).value() == "second");
        
        CHECK(map.size() == 1);
    }

    // ============================================
    // Find and Lookup Tests
    // ============================================

    TEST_CASE("hashmap_find_existing") {
        // Test finding existing keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.emplace(2, 200);
        map.emplace(3, 300);
        
        auto iter = map.find(2);
        CHECK(iter); // Evaluates to true in boolean context
        CHECK(iter.key() == 2);
        CHECK(iter.value() == 200);
    }

    TEST_CASE("hashmap_find_nonexistent") {
        // Test finding non-existent keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        auto iter = map.find(999);
        CHECK(!iter); // Evaluates to false in boolean context
    }

    // ============================================
    // Remove Operations Tests
    // ============================================

    TEST_CASE("hashmap_remove_existing") {
        // Test removing existing keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.emplace(2, 200);
        map.emplace(3, 300);
        
        CHECK(map.size() == 3);
        
        // Remove middle element
        map.remove(2);
        CHECK(map.size() == 2);
        CHECK(!map.find(2));
        CHECK(map.find(1));
        CHECK(map.find(3));
    }

    TEST_CASE("hashmap_remove_nonexistent") {
        // Test removing non-existent key (should not crash)
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        // This should be safe even if key doesn't exist
        map.remove(999);
        CHECK(map.size() == 1);
        CHECK(map.find(1));
    }

    TEST_CASE("hashmap_remove_all") {
        // Test removing all elements one by one
        vstd::HashMap<int, int> map;
        for (int i = 0; i < 10; i++) {
            map.emplace(i, i * 10);
        }
        CHECK(map.size() == 10);
        
        for (int i = 0; i < 10; i++) {
            map.remove(i);
        }
        
        CHECK(map.empty());
        CHECK(map.size() == 0);
    }

    // ============================================
    // Capacity Operations Tests
    // ============================================

    TEST_CASE("hashmap_reserve") {
        // Test reserving capacity
        vstd::HashMap<int, int> map;
        
        // Reserve space for 100 elements
        map.reserve(100);
        
        // Map should still be empty after reserve
        CHECK(map.empty());
        CHECK(map.size() == 0);
        
        // Adding elements should work normally
        for (int i = 0; i < 50; i++) {
            map.emplace(i, i);
        }
        CHECK(map.size() == 50);
    }

    TEST_CASE("hashmap_clear") {
        // Test clearing all elements
        vstd::HashMap<int, luisa::string> map;
        map.emplace(1, "one");
        map.emplace(2, "two");
        map.emplace(3, "three");
        
        CHECK(map.size() == 3);
        
        map.clear();
        
        CHECK(map.empty());
        CHECK(map.size() == 0);
        CHECK(!map.find(1));
        CHECK(!map.find(2));
        CHECK(!map.find(3));
    }

    TEST_CASE("hashmap_clear_empty") {
        // Test clearing an already empty map
        vstd::HashMap<int, int> map;
        CHECK(map.empty());
        
        map.clear(); // Should be safe
        CHECK(map.empty());
    }

    TEST_CASE("hashmap_size_and_empty") {
        // Test size() and empty() methods
        vstd::HashMap<int, int> map;
        
        CHECK(map.empty());
        CHECK(map.size() == 0);
        
        map.emplace(1, 10);
        CHECK(!map.empty());
        CHECK(map.size() == 1);
        
        map.emplace(2, 20);
        CHECK(map.size() == 2);
        
        map.remove(1);
        CHECK(map.size() == 1);
        
        map.remove(2);
        CHECK(map.empty());
        CHECK(map.size() == 0);
    }

    // ============================================
    // Complex Value Types Tests
    // ============================================

    TEST_CASE("hashmap_complex_value_type") {
        // Test with complex value types (vector)
        vstd::HashMap<int, luisa::vector<int>> map;
        
        luisa::vector<int> vec1 = {1, 2, 3};
        luisa::vector<int> vec2 = {4, 5, 6, 7};
        
        map.emplace(1, vec1);
        map.emplace(2, vec2);
        
        CHECK(map.size() == 2);
        CHECK(map.find(1).value().size() == 3);
        CHECK(map.find(2).value().size() == 4);
    }

    TEST_CASE("hashmap_string_key_and_value") {
        // Test with string keys and values
        vstd::HashMap<luisa::string, luisa::string> map;
        
        map.emplace("key1", "value1");
        map.emplace("key2", "value2");
        map.emplace("key3", "value3");
        
        CHECK(map.find("key1").value() == "value1");
        CHECK(map.find("key2").value() == "value2");
        CHECK(map.find("key3").value() == "value3");
    }

    // ============================================
    // Large Scale Tests
    // ============================================

    TEST_CASE("hashmap_large_insertion") {
        // Test with large number of elements
        vstd::HashMap<int, int> map;
        const int count = 10000;
        
        for (int i = 0; i < count; i++) {
            map.emplace(i, i * i);
        }
        
        CHECK(map.size() == count);
        
        // Verify all elements
        for (int i = 0; i < count; i++) {
            auto iter = map.find(i);
            CHECK(iter);
            CHECK(iter.value() == i * i);
        }
    }

    TEST_CASE("hashmap_large_remove") {
        // Test removing large number of elements
        vstd::HashMap<int, int> map;
        const int count = 1000;
        
        for (int i = 0; i < count; i++) {
            map.emplace(i, i);
        }
        
        // Remove even numbers
        for (int i = 0; i < count; i += 2) {
            map.remove(i);
        }
        
        CHECK(map.size() == count / 2);
        
        // Verify odd numbers still exist
        for (int i = 1; i < count; i += 2) {
            CHECK(map.find(i));
        }
        
        // Verify even numbers are gone
        for (int i = 0; i < count; i += 2) {
            CHECK(!map.find(i));
        }
    }

    // ============================================
    // Indexer Tests
    // ============================================

    TEST_CASE("hashmap_indexer_boolean_conversion") {
        // Test indexer boolean conversion
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        auto found = map.find(1);
        auto not_found = map.find(999);
        
        // Indexer converts to bool
        CHECK(found);        // True when key exists
        CHECK(!not_found);   // False when key doesn't exist
    }

    TEST_CASE("hashmap_indexer_key_value_access") {
        // Test indexer key() and value() methods
        vstd::HashMap<luisa::string, double> map;
        map.emplace("pi", 3.14159);
        
        auto iter = map.find("pi");
        CHECK(iter.key() == "pi");
        CHECK(iter.value() == doctest::Approx(3.14159));
        
        // Modify value through indexer
        iter.value() = 2.71828;
        CHECK(map.find("pi").value() == doctest::Approx(2.71828));
    }

    // ============================================
    // Edge Cases Tests
    // ============================================

    TEST_CASE("hashmap_zero_as_key") {
        // Test using 0 as a key
        vstd::HashMap<int, int> map;
        map.emplace(0, 999);
        
        CHECK(map.find(0));
        CHECK(map.find(0).value() == 999);
    }

    TEST_CASE("hashmap_negative_keys") {
        // Test using negative numbers as keys
        vstd::HashMap<int, int> map;
        map.emplace(-1, 100);
        map.emplace(-100, 200);
        map.emplace(-999, 300);
        
        CHECK(map.size() == 3);
        CHECK(map.find(-1).value() == 100);
        CHECK(map.find(-100).value() == 200);
        CHECK(map.find(-999).value() == 300);
    }

    TEST_CASE("hashmap_empty_string_key") {
        // Test using empty string as key
        vstd::HashMap<luisa::string, int> map;
        map.emplace("", 42);
        
        CHECK(map.find(""));
        CHECK(map.find("").value() == 42);
    }

    TEST_CASE("hashmap_reinsert_after_remove") {
        // Test re-inserting after removal
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.remove(1);
        CHECK(!map.find(1));
        
        map.emplace(1, 200);
        CHECK(map.find(1));
        CHECK(map.find(1).value() == 200);
    }

    // ============================================
    // Custom Hash and Equality Tests
    // ============================================

    struct CustomKey {
        int x;
        int y;
        
        bool operator==(const CustomKey& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct CustomKeyHash {
        size_t operator()(const CustomKey& key) const {
            return std::hash<int>()(key.x) ^ (std::hash<int>()(key.y) << 1);
        }
    };

    TEST_CASE("hashmap_custom_key_type") {
        // Test with custom key type and custom hash/equality
        vstd::HashMap<CustomKey, luisa::string, CustomKeyHash> map;
        
        CustomKey key1{1, 2};
        CustomKey key2{3, 4};
        
        map.emplace(key1, "point_1_2");
        map.emplace(key2, "point_3_4");
        
        CHECK(map.size() == 2);
        CHECK(map.find(key1).value() == "point_1_2");
        CHECK(map.find(key2).value() == "point_3_4");
        
        // Test with equivalent key
        CustomKey key1_copy{1, 2};
        CHECK(map.find(key1_copy).value() == "point_1_2");
    }
}
