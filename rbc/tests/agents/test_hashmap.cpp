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


    // ============================================
    // Basic Construction Tests
    // ============================================

    "hashmap_default_construction"_test = [] {
        // Test default constructor creates an empty hash map
        vstd::HashMap<int, int> map;
        expect(static_cast<bool>(map.size() == 0));
        expect(static_cast<bool>(map.empty()));
    };

    "hashmap_with_string_keys"_test = [] {
        // Test HashMap with string keys and int values
        vstd::HashMap<luisa::string, int> map;
        expect(static_cast<bool>(map.empty()));
        
        map.emplace("key1", 100);
        expect(static_cast<bool>(map.size() == 1));
        expect(static_cast<bool>(!map.empty()));
    };

    // ============================================
    // Emplace Operations Tests
    // ============================================

    "hashmap_emplace_basic"_test = [] {
        // Test basic emplace operation
        vstd::HashMap<int, luisa::string> map;
        
        // Emplace key-value pairs
        map.emplace(1, "one");
        map.emplace(2, "two");
        map.emplace(3, "three");
        
        expect(static_cast<bool>(map.size() == 3));
        
        // Verify all keys exist
        auto iter1 = map.find(1);
        expect(static_cast<bool>(iter1));
        expect(static_cast<bool>(iter1.key() == 1));
        expect(static_cast<bool>(iter1.value() == "one"));
        
        auto iter2 = map.find(2);
        expect(static_cast<bool>(iter2));
        expect(static_cast<bool>(iter2.value() == "two"));
        
        auto iter3 = map.find(3);
        expect(static_cast<bool>(iter3));
        expect(static_cast<bool>(iter3.value() == "three"));
    };

    "hashmap_emplace_no_overwrite"_test = [] {
        // Test that emplace does not overwrite existing values
        // (this is the behavior of vstd::HashMap emplace)
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        expect(static_cast<bool>(map.find(1).value() == 100));
        
        // Emplace with same key should NOT overwrite
        map.emplace(1, 200);
        expect(static_cast<bool>(map.size() == 1));
        expect(static_cast<bool>(map.find(1).value() == 100)); // Value unchanged
    };

    "hashmap_try_emplace_basic"_test = [] {
        // Test try_emplace - only inserts if key doesn't exist
        vstd::HashMap<luisa::string, int> map;
        
        // First try_emplace should succeed
        auto result1 = map.try_emplace("key1", 100);
        expect(static_cast<bool>(result1.second)); // Insertion happened
        expect(static_cast<bool>(result1.first));
        expect(static_cast<bool>(result1.first.key() == "key1"));
        expect(static_cast<bool>(result1.first.value() == 100));
        
        // Second try_emplace with same key should fail
        auto result2 = map.try_emplace("key1", 200);
        expect(static_cast<bool>(!result2.second)); // No insertion
        expect(static_cast<bool>(result2.first));
        expect(static_cast<bool>(result2.first.value() == 100)); // Value unchanged
        
        expect(static_cast<bool>(map.size() == 1));
    };

    "hashmap_try_emplace_multiple"_test = [] {
        // Test try_emplace with multiple keys
        vstd::HashMap<int, double> map;
        
        map.try_emplace(1, 1.1);
        map.try_emplace(2, 2.2);
        map.try_emplace(3, 3.3);
        
        expect(static_cast<bool>(map.size() == 3));
        
        // Verify values
        expect(static_cast<bool>(map.find(1).value() == Approx(1.1)));
        expect(static_cast<bool>(map.find(2).value() == Approx(2.2)));
        expect(static_cast<bool>(map.find(3).value() == Approx(3.3)));
    };

    "hashmap_force_emplace"_test = [] {
        // Test force_emplace - always inserts/replaces
        vstd::HashMap<int, luisa::string> map;
        
        map.force_emplace(1, "first");
        expect(static_cast<bool>(map.find(1).value() == "first"));
        
        // force_emplace should overwrite existing value
        map.force_emplace(1, "second");
        expect(static_cast<bool>(map.find(1).value() == "second"));
        
        expect(static_cast<bool>(map.size() == 1));
    };

    // ============================================
    // Find and Lookup Tests
    // ============================================

    "hashmap_find_existing"_test = [] {
        // Test finding existing keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.emplace(2, 200);
        map.emplace(3, 300);
        
        auto iter = map.find(2);
        expect(static_cast<bool>(iter)); // Evaluates to true in boolean context
        expect(static_cast<bool>(iter.key() == 2));
        expect(static_cast<bool>(iter.value() == 200));
    };

    "hashmap_find_nonexistent"_test = [] {
        // Test finding non-existent keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        auto iter = map.find(999);
        expect(static_cast<bool>(!iter)); // Evaluates to false in boolean context
    };

    // ============================================
    // Remove Operations Tests
    // ============================================

    "hashmap_remove_existing"_test = [] {
        // Test removing existing keys
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.emplace(2, 200);
        map.emplace(3, 300);
        
        expect(static_cast<bool>(map.size() == 3));
        
        // Remove middle element
        map.remove(2);
        expect(static_cast<bool>(map.size() == 2));
        expect(static_cast<bool>(!map.find(2)));
        expect(static_cast<bool>(map.find(1)));
        expect(static_cast<bool>(map.find(3)));
    };

    "hashmap_remove_nonexistent"_test = [] {
        // Test removing non-existent key (should not crash)
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        // This should be safe even if key doesn't exist
        map.remove(999);
        expect(static_cast<bool>(map.size() == 1));
        expect(static_cast<bool>(map.find(1)));
    };

    "hashmap_remove_all"_test = [] {
        // Test removing all elements one by one
        vstd::HashMap<int, int> map;
        for (int i = 0; i < 10; i++) {
            map.emplace(i, i * 10);
        }
        expect(static_cast<bool>(map.size() == 10));
        
        for (int i = 0; i < 10; i++) {
            map.remove(i);
        }
        
        expect(static_cast<bool>(map.empty()));
        expect(static_cast<bool>(map.size() == 0));
    };

    // ============================================
    // Capacity Operations Tests
    // ============================================

    "hashmap_reserve"_test = [] {
        // Test reserving capacity
        vstd::HashMap<int, int> map;
        
        // Reserve space for 100 elements
        map.reserve(100);
        
        // Map should still be empty after reserve
        expect(static_cast<bool>(map.empty()));
        expect(static_cast<bool>(map.size() == 0));
        
        // Adding elements should work normally
        for (int i = 0; i < 50; i++) {
            map.emplace(i, i);
        }
        expect(static_cast<bool>(map.size() == 50));
    };

    "hashmap_clear"_test = [] {
        // Test clearing all elements
        vstd::HashMap<int, luisa::string> map;
        map.emplace(1, "one");
        map.emplace(2, "two");
        map.emplace(3, "three");
        
        expect(static_cast<bool>(map.size() == 3));
        
        map.clear();
        
        expect(static_cast<bool>(map.empty()));
        expect(static_cast<bool>(map.size() == 0));
        expect(static_cast<bool>(!map.find(1)));
        expect(static_cast<bool>(!map.find(2)));
        expect(static_cast<bool>(!map.find(3)));
    };

    "hashmap_clear_empty"_test = [] {
        // Test clearing an already empty map
        vstd::HashMap<int, int> map;
        expect(static_cast<bool>(map.empty()));
        
        map.clear(); // Should be safe
        expect(static_cast<bool>(map.empty()));
    };

    "hashmap_size_and_empty"_test = [] {
        // Test size() and empty() methods
        vstd::HashMap<int, int> map;
        
        expect(static_cast<bool>(map.empty()));
        expect(static_cast<bool>(map.size() == 0));
        
        map.emplace(1, 10);
        expect(static_cast<bool>(!map.empty()));
        expect(static_cast<bool>(map.size() == 1));
        
        map.emplace(2, 20);
        expect(static_cast<bool>(map.size() == 2));
        
        map.remove(1);
        expect(static_cast<bool>(map.size() == 1));
        
        map.remove(2);
        expect(static_cast<bool>(map.empty()));
        expect(static_cast<bool>(map.size() == 0));
    };

    // ============================================
    // Complex Value Types Tests
    // ============================================

    "hashmap_complex_value_type"_test = [] {
        // Test with complex value types (vector)
        vstd::HashMap<int, luisa::vector<int>> map;
        
        luisa::vector<int> vec1 = {1, 2, 3};
        luisa::vector<int> vec2 = {4, 5, 6, 7};
        
        map.emplace(1, vec1);
        map.emplace(2, vec2);
        
        expect(static_cast<bool>(map.size() == 2));
        expect(static_cast<bool>(map.find(1).value().size() == 3));
        expect(static_cast<bool>(map.find(2).value().size() == 4));
    };

    "hashmap_string_key_and_value"_test = [] {
        // Test with string keys and values
        vstd::HashMap<luisa::string, luisa::string> map;
        
        map.emplace("key1", "value1");
        map.emplace("key2", "value2");
        map.emplace("key3", "value3");
        
        expect(static_cast<bool>(map.find("key1").value() == "value1"));
        expect(static_cast<bool>(map.find("key2").value() == "value2"));
        expect(static_cast<bool>(map.find("key3").value() == "value3"));
    };

    // ============================================
    // Large Scale Tests
    // ============================================

    "hashmap_large_insertion"_test = [] {
        // Test with large number of elements
        vstd::HashMap<int, int> map;
        const int count = 10000;
        
        for (int i = 0; i < count; i++) {
            map.emplace(i, i * i);
        }
        
        expect(static_cast<bool>(map.size() == count));
        
        // Verify all elements
        for (int i = 0; i < count; i++) {
            auto iter = map.find(i);
            expect(static_cast<bool>(iter));
            expect(static_cast<bool>(iter.value() == i * i));
        }
    };

    "hashmap_large_remove"_test = [] {
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
        
        expect(static_cast<bool>(map.size() == count / 2));
        
        // Verify odd numbers still exist
        for (int i = 1; i < count; i += 2) {
            expect(static_cast<bool>(map.find(i)));
        }
        
        // Verify even numbers are gone
        for (int i = 0; i < count; i += 2) {
            expect(static_cast<bool>(!map.find(i)));
        }
    };

    // ============================================
    // Indexer Tests
    // ============================================

    "hashmap_indexer_boolean_conversion"_test = [] {
        // Test indexer boolean conversion
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        
        auto found = map.find(1);
        auto not_found = map.find(999);
        
        // Indexer converts to bool
        expect(static_cast<bool>(found));        // True when key exists
        expect(static_cast<bool>(!not_found));   // False when key doesn't exist
    };

    "hashmap_indexer_key_value_access"_test = [] {
        // Test indexer key() and value() methods
        vstd::HashMap<luisa::string, double> map;
        map.emplace("pi", 3.14159);
        
        auto iter = map.find("pi");
        expect(static_cast<bool>(iter.key() == "pi"));
        expect(static_cast<bool>(iter.value() == Approx(3.14159)));
        
        // Modify value through indexer
        iter.value() = 2.71828;
        expect(static_cast<bool>(map.find("pi").value() == Approx(2.71828)));
    };

    // ============================================
    // Edge Cases Tests
    // ============================================

    "hashmap_zero_as_key"_test = [] {
        // Test using 0 as a key
        vstd::HashMap<int, int> map;
        map.emplace(0, 999);
        
        expect(static_cast<bool>(map.find(0)));
        expect(static_cast<bool>(map.find(0).value() == 999));
    };

    "hashmap_negative_keys"_test = [] {
        // Test using negative numbers as keys
        vstd::HashMap<int, int> map;
        map.emplace(-1, 100);
        map.emplace(-100, 200);
        map.emplace(-999, 300);
        
        expect(static_cast<bool>(map.size() == 3));
        expect(static_cast<bool>(map.find(-1).value() == 100));
        expect(static_cast<bool>(map.find(-100).value() == 200));
        expect(static_cast<bool>(map.find(-999).value() == 300));
    };

    "hashmap_empty_string_key"_test = [] {
        // Test using empty string as key
        vstd::HashMap<luisa::string, int> map;
        map.emplace("", 42);
        
        expect(static_cast<bool>(map.find("")));
        expect(static_cast<bool>(map.find("").value() == 42));
    };

    "hashmap_reinsert_after_remove"_test = [] {
        // Test re-inserting after removal
        vstd::HashMap<int, int> map;
        map.emplace(1, 100);
        map.remove(1);
        expect(static_cast<bool>(!map.find(1)));
        
        map.emplace(1, 200);
        expect(static_cast<bool>(map.find(1)));
        expect(static_cast<bool>(map.find(1).value() == 200));
    };

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

    "hashmap_custom_key_type"_test = [] {
        // Test with custom key type and custom hash/equality
        vstd::HashMap<CustomKey, luisa::string, CustomKeyHash> map;
        
        CustomKey key1{1, 2};
        CustomKey key2{3, 4};
        
        map.emplace(key1, "point_1_2");
        map.emplace(key2, "point_3_4");
        
        expect(static_cast<bool>(map.size() == 2));
        expect(static_cast<bool>(map.find(key1).value() == "point_1_2"));
        expect(static_cast<bool>(map.find(key2).value() == "point_3_4"));
        
        // Test with equivalent key
        CustomKey key1_copy{1, 2};
        expect(static_cast<bool>(map.find(key1_copy).value() == "point_1_2"));
    };