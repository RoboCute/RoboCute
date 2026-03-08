/**
 * @file test_optional.cpp
 * @brief Comprehensive test suite for vstd::optional
 * 
 * This file tests all APIs of vstd::optional including:
 * - Constructors (default, nullopt, value, copy, move, in_place)
 * - Assignment operators (nullopt, copy, move, value)
 * - Observers (has_value, operator bool, value, value_or, operator*, operator->)
 * - Modifiers (emplace, reset, swap)
 * - Comparison operators (==, !=, <, <=, >, >=)
 * - Utility functions (make_optional, swap)
 */

#include <luisa/vstl/common.h>
#include <luisa/core/stl/optional.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>
#include <EASTL/internal/in_place_t.h>  // for eastl::in_place
#include "../_framework/test_util.h"

// Test counter for tracking constructor/destructor calls
struct TestCounter {
    static int constructor_count;
    static int destructor_count;
    static int copy_constructor_count;
    static int move_constructor_count;
    static int copy_assignment_count;
    static int move_assignment_count;
    
    int value;
    
    TestCounter() : value(0) { constructor_count++; }
    explicit TestCounter(int v) : value(v) { constructor_count++; }
    TestCounter(const TestCounter& other) : value(other.value) { copy_constructor_count++; }
    TestCounter(TestCounter&& other) noexcept : value(other.value) { move_constructor_count++; }
    TestCounter& operator=(const TestCounter& other) { 
        value = other.value; 
        copy_assignment_count++; 
        return *this; 
    }
    TestCounter& operator=(TestCounter&& other) noexcept { 
        value = other.value; 
        move_assignment_count++; 
        return *this; 
    }
    ~TestCounter() { destructor_count++; }
    
    static void reset() {
        constructor_count = 0;
        destructor_count = 0;
        copy_constructor_count = 0;
        move_constructor_count = 0;
        copy_assignment_count = 0;
        move_assignment_count = 0;
    }
    
    bool operator==(const TestCounter& other) const { return value == other.value; }
    bool operator<(const TestCounter& other) const { return value < other.value; }
};

int TestCounter::constructor_count = 0;
int TestCounter::destructor_count = 0;
int TestCounter::copy_constructor_count = 0;
int TestCounter::move_constructor_count = 0;
int TestCounter::copy_assignment_count = 0;
int TestCounter::move_assignment_count = 0;

TEST_SUITE("Optional") {

    // ============================================
    // Construction Tests
    // ============================================

    TEST_CASE("optional_default_construction") {
        // Default constructor creates an empty optional
        luisa::optional<int> opt;
        CHECK(!opt.has_value());
        CHECK(!opt);
    }

    TEST_CASE("optional_nullopt_construction") {
        // nullopt constructor creates an empty optional
        luisa::optional<int> opt(luisa::nullopt);
        CHECK(!opt.has_value());
        CHECK(!opt);
    }

    TEST_CASE("optional_value_construction_lvalue") {
        // Construct with lvalue
        int value = 42;
        luisa::optional<int> opt(value);
        CHECK(opt.has_value());
        CHECK(opt.value() == 42);
    }

    TEST_CASE("optional_value_construction_rvalue") {
        // Construct with rvalue
        luisa::optional<int> opt(100);
        CHECK(opt.has_value());
        CHECK(*opt == 100);
    }

    TEST_CASE("optional_value_conversion") {
        // Test implicit conversion from value type
        luisa::optional<double> opt = 3.14;
        CHECK(opt.has_value());
        CHECK(*opt == doctest::Approx(3.14));
    }

    TEST_CASE("optional_copy_construction") {
        // Test copy constructor
        luisa::optional<int> opt1(42);
        luisa::optional<int> opt2(opt1);
        CHECK(opt2.has_value());
        CHECK(*opt2 == 42);
        // Verify independence
        *opt1 = 100;
        CHECK(*opt2 == 42);
    }

    TEST_CASE("optional_move_construction") {
        // Test move constructor
        luisa::optional<luisa::string> opt1(luisa::string("hello"));
        luisa::optional<luisa::string> opt2(std::move(opt1));
        CHECK(opt2.has_value());
        CHECK(*opt2 == "hello");
    }

    TEST_CASE("optional_inplace_construction") {
        // Test in_place construction with arguments
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 42);
            CHECK(opt.has_value());
            CHECK(opt->value == 42);
            // Should only call the int constructor
            CHECK(TestCounter::constructor_count == 1);
            CHECK(TestCounter::copy_constructor_count == 0);
        }
    }

    TEST_CASE("optional_inplace_construction_initializer_list") {
        // Test in_place construction with initializer list
        luisa::optional<luisa::vector<int>> opt(eastl::in_place, luisa::vector<int>{1, 2, 3, 4, 5});
        CHECK(opt.has_value());
        CHECK(opt->size() == 5);
        CHECK((*opt)[0] == 1);
        CHECK((*opt)[4] == 5);
    }

    // ============================================
    // Assignment Tests
    // ============================================

    TEST_CASE("optional_nullopt_assignment") {
        // Assign nullopt to clear value
        luisa::optional<int> opt(42);
        CHECK(opt.has_value());
        opt = luisa::nullopt;
        CHECK(!opt.has_value());
    }

    TEST_CASE("optional_copy_assignment") {
        // Test copy assignment
        luisa::optional<int> opt1(100);
        luisa::optional<int> opt2;
        opt2 = opt1;
        CHECK(opt2.has_value());
        CHECK(*opt2 == 100);
    }

    TEST_CASE("optional_move_assignment") {
        // Test move assignment
        luisa::optional<luisa::string> opt1(luisa::string("move me"));
        luisa::optional<luisa::string> opt2;
        opt2 = std::move(opt1);
        CHECK(opt2.has_value());
        CHECK(*opt2 == "move me");
    }

    TEST_CASE("optional_value_assignment") {
        // Test value assignment to empty optional
        luisa::optional<int> opt;
        opt = 200;
        CHECK(opt.has_value());
        CHECK(*opt == 200);
        
        // Test value assignment to existing value (overwrite)
        opt = 300;
        CHECK(opt.has_value());
        CHECK(*opt == 300);
    }

    TEST_CASE("optional_self_assignment") {
        // Test self copy assignment
        luisa::optional<int> opt(42);
        opt = opt;  // Self assignment
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    // ============================================
    // Observer Tests
    // ============================================

    TEST_CASE("optional_has_value") {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2;
        
        CHECK(opt1.has_value());
        CHECK(!opt2.has_value());
    }

    TEST_CASE("optional_operator_bool") {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2;
        
        CHECK(static_cast<bool>(opt1));
        CHECK(!static_cast<bool>(opt2));
    }

    TEST_CASE("optional_value_accessor") {
        luisa::optional<int> opt(42);
        
        // Non-const value()
        CHECK(opt.value() == 42);
        opt.value() = 100;
        CHECK(opt.value() == 100);
        
        // Const value()
        const luisa::optional<int>& const_opt = opt;
        CHECK(const_opt.value() == 100);
    }

    TEST_CASE("optional_dereference_operator") {
        luisa::optional<int> opt(42);
        
        // operator*
        CHECK(*opt == 42);
        *opt = 100;
        CHECK(*opt == 100);
    }

    TEST_CASE("optional_arrow_operator") {
        luisa::optional<luisa::string> opt("hello");
        
        // operator->
        CHECK(opt->size() == 5);
        opt->clear();
        CHECK(opt->empty());
    }

    TEST_CASE("optional_value_or") {
        luisa::optional<int> opt_val(42);
        luisa::optional<int> opt_empty;
        
        // value_or on valued optional
        CHECK(opt_val.value_or(999) == 42);
        
        // value_or on empty optional
        CHECK(opt_empty.value_or(999) == 999);
    }

    // ============================================
    // Modifier Tests
    // ============================================

    TEST_CASE("optional_emplace_empty") {
        // Emplace on empty optional
        luisa::optional<luisa::string> opt;
        opt.emplace("hello world");
        CHECK(opt.has_value());
        CHECK(*opt == "hello world");
    }

    TEST_CASE("optional_emplace_replace") {
        // Emplace on existing value - should replace
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 10);
            CHECK(TestCounter::constructor_count == 1);
            
            TestCounter::reset();
            opt.emplace(20);
            CHECK(opt->value == 20);
            // Should destroy old and construct new
            CHECK(TestCounter::destructor_count == 1);
            CHECK(TestCounter::constructor_count == 1);
        }
    }

    TEST_CASE("optional_emplace_multiple_args") {
        // Emplace with multiple arguments
        luisa::optional<luisa::string> opt;
        opt.emplace(5, 'x');
        CHECK(opt.has_value());
        CHECK(*opt == "xxxxx");
    }

    TEST_CASE("optional_reset") {
        // Reset a valued optional
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 42);
            CHECK(opt.has_value());
            
            opt.reset();
            CHECK(!opt.has_value());
            CHECK(TestCounter::destructor_count == 1);
        }
    }

    TEST_CASE("optional_reset_empty") {
        // Reset an empty optional (should be safe)
        luisa::optional<int> opt;
        opt.reset();
        CHECK(!opt.has_value());
    }

    TEST_CASE("optional_swap_both_valued") {
        // Swap two valued optionals
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        opt1.swap(opt2);
        CHECK(*opt1 == 20);
        CHECK(*opt2 == 10);
    }

    TEST_CASE("optional_swap_one_empty") {
        // Swap with one empty
        luisa::optional<int> opt1(30);
        luisa::optional<int> opt2;
        opt1.swap(opt2);
        CHECK(!opt1.has_value());
        CHECK(opt2.has_value());
        CHECK(*opt2 == 30);
    }

    TEST_CASE("optional_swap_both_empty") {
        // Swap two empty optionals
        luisa::optional<int> opt1;
        luisa::optional<int> opt2;
        opt1.swap(opt2);
        CHECK(!opt1.has_value());
        CHECK(!opt2.has_value());
    }

    TEST_CASE("optional_global_swap") {
        // Test global swap function
        luisa::optional<int> opt1(100);
        luisa::optional<int> opt2(200);
        swap(opt1, opt2);
        CHECK(*opt1 == 200);
        CHECK(*opt2 == 100);
    }

    // ============================================
    // Comparison Tests - Optional vs Optional
    // ============================================

    TEST_CASE("optional_equality_both_valued") {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(10);
        luisa::optional<int> opt3(20);
        
        CHECK(opt1 == opt2);
        CHECK(!(opt1 == opt3));
    }

    TEST_CASE("optional_inequality_both_valued") {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        
        CHECK(opt1 != opt2);
    }

    TEST_CASE("optional_equality_empty") {
        luisa::optional<int> opt1;
        luisa::optional<int> opt2;
        luisa::optional<int> opt3(10);
        
        // Empty == Empty
        CHECK(opt1 == opt2);
        // Empty != Valued
        CHECK(!(opt1 == opt3));
        CHECK(opt1 != opt3);
    }

    TEST_CASE("optional_comparison_nullopt") {
        luisa::optional<int> opt_empty;
        luisa::optional<int> opt_val(10);
        
        CHECK(opt_empty == luisa::nullopt);
        CHECK(luisa::nullopt == opt_empty);
        CHECK(opt_val != luisa::nullopt);
        CHECK(luisa::nullopt != opt_val);
    }

    TEST_CASE("optional_ordering_both_valued") {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        
        CHECK(opt1 < opt2);
        CHECK(opt2 > opt1);
        CHECK(opt1 <= opt2);
        CHECK(opt2 >= opt1);
        CHECK(opt1 <= opt1);  // Equality case
        CHECK(opt1 >= opt1);  // Equality case
    }

    TEST_CASE("optional_ordering_with_empty") {
        luisa::optional<int> opt_empty;
        luisa::optional<int> opt_val(10);
        
        // Empty < Valued
        CHECK(opt_empty < opt_val);
        CHECK(opt_val > opt_empty);
        
        // Empty <= Empty
        CHECK(opt_empty <= opt_empty);
        CHECK(opt_empty >= opt_empty);
    }

    // ============================================
    // Comparison Tests - Optional vs Value
    // ============================================

    TEST_CASE("optional_value_equality") {
        luisa::optional<int> opt(10);
        
        CHECK(opt == 10);
        CHECK(10 == opt);
        CHECK(!(opt == 20));
        CHECK(opt != 20);
        CHECK(20 != opt);
    }

    TEST_CASE("optional_value_ordering") {
        luisa::optional<int> opt(10);
        
        CHECK(opt < 20);
        CHECK(5 < opt);
        CHECK(opt > 5);
        CHECK(20 > opt);
        CHECK(opt <= 10);
        CHECK(opt <= 20);
        CHECK(opt >= 10);
        CHECK(opt >= 5);
    }

    TEST_CASE("optional_empty_value_comparison") {
        luisa::optional<int> opt_empty;
        
        CHECK(!(opt_empty == 10));
        CHECK(!(10 == opt_empty));
        CHECK(opt_empty != 10);
        CHECK(opt_empty < 10);
        CHECK(!(opt_empty > 10));
    }

    // ============================================
    // Utility Function Tests
    // ============================================

    TEST_CASE("optional_make_optional_value") {
        auto opt = luisa::make_optional(42);
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    TEST_CASE("optional_make_optional_args") {
        // make_optional with constructor arguments
        auto opt = luisa::make_optional<luisa::string>(luisa::string(5, 'x'));
        CHECK(opt.has_value());
        CHECK(*opt == "xxxxx");
    }

    TEST_CASE("optional_make_optional_inplace") {
        // make_optional in-place construction
        struct Point {
            int x, y;
            Point(int _x, int _y) : x(_x), y(_y) {}
        };
        auto opt = luisa::make_optional<Point>(3, 4);
        (void)opt;
        CHECK(opt.has_value());
        CHECK(opt->x == 3);
        CHECK(opt->y == 4);
    }

    // ============================================
    // Complex Type Tests
    // ============================================

    TEST_CASE("optional_complex_string") {
        // Test with string type
        luisa::optional<luisa::string> opt;
        CHECK(!opt.has_value());
        
        opt = luisa::string("hello world");
        CHECK(opt.has_value());
        CHECK(*opt == "hello world");
        
        opt.reset();
        CHECK(!opt.has_value());
    }

    TEST_CASE("optional_complex_vector") {
        // Test with vector type
        luisa::optional<luisa::vector<int>> opt;
        CHECK(!opt.has_value());
        
        opt.emplace();
        opt->push_back(1);
        opt->push_back(2);
        opt->push_back(3);
        
        CHECK(opt->size() == 3);
        CHECK((*opt)[0] == 1);
        CHECK((*opt)[2] == 3);
    }

    TEST_CASE("optional_custom_class") {
        // Test with custom class
        struct Point {
            int x, y;
            Point() : x(0), y(0) {}
            Point(int _x, int _y) : x(_x), y(_y) {}
            bool operator==(const Point& other) const {
                return x == other.x && y == other.y;
            }
        };
        
        luisa::optional<Point> pt;
        CHECK(!pt.has_value());
        
        pt.emplace(3, 4);
        CHECK(pt.has_value());
        CHECK(pt->x == 3);
        CHECK(pt->y == 4);
    }

    // ============================================
    // Edge Cases
    // ============================================

    TEST_CASE("optional_chained_ops") {
        // Test chaining operations
        luisa::optional<luisa::string> opt;
        opt = luisa::string("test");
        opt->append("_string");
        CHECK(*opt == "test_string");
    }

    TEST_CASE("optional_zero_as_value") {
        // Test using 0 as a valid value
        luisa::optional<int> opt(0);
        CHECK(opt.has_value());
        CHECK(*opt == 0);
        CHECK(opt != luisa::nullopt);
    }

    TEST_CASE("optional_false_as_value") {
        // Test using false as a valid value
        luisa::optional<bool> opt(false);
        CHECK(opt.has_value());
        CHECK(*opt == false);
        CHECK(opt != luisa::nullopt);
    }

    TEST_CASE("optional_pointer_type") {
        // Test with pointer type
        int value = 42;
        luisa::optional<int*> opt(&value);
        CHECK(opt.has_value());
        CHECK(**opt == 42);
    }

    TEST_CASE("optional_move_only_type") {
        // Test with a move-only type
        struct MoveOnly {
            int value;
            explicit MoveOnly(int v) : value(v) {}
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly(MoveOnly&&) = default;
            MoveOnly& operator=(const MoveOnly&) = delete;
            MoveOnly& operator=(MoveOnly&&) = default;
        };
        
        luisa::optional<MoveOnly> opt(eastl::in_place, 42);
        CHECK(opt.has_value());
        CHECK(opt->value == 42);
        
        luisa::optional<MoveOnly> opt2(std::move(opt));
        CHECK(opt2.has_value());
        CHECK(opt2->value == 42);
    }

    TEST_CASE("optional_multiple_resets") {
        // Test multiple resets
        luisa::optional<int> opt(1);
        opt.reset();
        opt.reset();
        opt.reset();
        CHECK(!opt.has_value());
    }

    TEST_CASE("optional_reassign_after_reset") {
        // Test reassigning after reset
        luisa::optional<int> opt(1);
        opt.reset();
        CHECK(!opt.has_value());
        
        opt = 2;
        CHECK(opt.has_value());
        CHECK(*opt == 2);
        
        opt = 3;
        CHECK(*opt == 3);
    }
}
