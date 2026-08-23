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


    // ============================================
    // Construction Tests
    // ============================================

    "optional_default_construction"_test = [] {
        // Default constructor creates an empty optional
        luisa::optional<int> opt;
        expect(static_cast<bool>(!opt.has_value()));
        expect(static_cast<bool>(!opt));
    };

    "optional_nullopt_construction"_test = [] {
        // nullopt constructor creates an empty optional
        luisa::optional<int> opt(luisa::nullopt);
        expect(static_cast<bool>(!opt.has_value()));
        expect(static_cast<bool>(!opt));
    };

    "optional_value_construction_lvalue"_test = [] {
        // Construct with lvalue
        int value = 42;
        luisa::optional<int> opt(value);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(opt.value() == 42));
    };

    "optional_value_construction_rvalue"_test = [] {
        // Construct with rvalue
        luisa::optional<int> opt(100);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 100));
    };

    "optional_value_conversion"_test = [] {
        // Test implicit conversion from value type
        luisa::optional<double> opt = 3.14;
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == Approx(3.14)));
    };

    "optional_copy_construction"_test = [] {
        // Test copy constructor
        luisa::optional<int> opt1(42);
        luisa::optional<int> opt2(opt1);
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(*opt2 == 42));
        // Verify independence
        *opt1 = 100;
        expect(static_cast<bool>(*opt2 == 42));
    };

    "optional_move_construction"_test = [] {
        // Test move constructor
        luisa::optional<luisa::string> opt1(luisa::string("hello"));
        luisa::optional<luisa::string> opt2(std::move(opt1));
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(*opt2 == "hello"));
    };

    "optional_inplace_construction"_test = [] {
        // Test in_place construction with arguments
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 42);
            expect(static_cast<bool>(opt.has_value()));
            expect(static_cast<bool>(opt->value == 42));
            // Should only call the int constructor
            expect(static_cast<bool>(TestCounter::constructor_count == 1));
            expect(static_cast<bool>(TestCounter::copy_constructor_count == 0));
        }
    };

    "optional_inplace_construction_initializer_list"_test = [] {
        // Test in_place construction with initializer list
        luisa::optional<luisa::vector<int>> opt(eastl::in_place, luisa::vector<int>{1, 2, 3, 4, 5});
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(opt->size() == 5));
        expect(static_cast<bool>((*opt)[0] == 1));
        expect(static_cast<bool>((*opt)[4] == 5));
    };

    // ============================================
    // Assignment Tests
    // ============================================

    "optional_nullopt_assignment"_test = [] {
        // Assign nullopt to clear value
        luisa::optional<int> opt(42);
        expect(static_cast<bool>(opt.has_value()));
        opt = luisa::nullopt;
        expect(static_cast<bool>(!opt.has_value()));
    };

    "optional_copy_assignment"_test = [] {
        // Test copy assignment
        luisa::optional<int> opt1(100);
        luisa::optional<int> opt2;
        opt2 = opt1;
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(*opt2 == 100));
    };

    "optional_move_assignment"_test = [] {
        // Test move assignment
        luisa::optional<luisa::string> opt1(luisa::string("move me"));
        luisa::optional<luisa::string> opt2;
        opt2 = std::move(opt1);
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(*opt2 == "move me"));
    };

    "optional_value_assignment"_test = [] {
        // Test value assignment to empty optional
        luisa::optional<int> opt;
        opt = 200;
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 200));
        
        // Test value assignment to existing value (overwrite)
        opt = 300;
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 300));
    };

    "optional_self_assignment"_test = [] {
        // Test self copy assignment
        luisa::optional<int> opt(42);
        opt = opt;  // Self assignment
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 42));
    };

    // ============================================
    // Observer Tests
    // ============================================

    "optional_has_value"_test = [] {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2;
        
        expect(static_cast<bool>(opt1.has_value()));
        expect(static_cast<bool>(!opt2.has_value()));
    };

    "optional_operator_bool"_test = [] {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2;
        
        expect(static_cast<bool>(static_cast<bool>(opt1)));
        expect(static_cast<bool>(!static_cast<bool>(opt2)));
    };

    "optional_value_accessor"_test = [] {
        luisa::optional<int> opt(42);
        
        // Non-const value()
        expect(static_cast<bool>(opt.value() == 42));
        opt.value() = 100;
        expect(static_cast<bool>(opt.value() == 100));
        
        // Const value()
        const luisa::optional<int>& const_opt = opt;
        expect(static_cast<bool>(const_opt.value() == 100));
    };

    "optional_dereference_operator"_test = [] {
        luisa::optional<int> opt(42);
        
        // operator*
        expect(static_cast<bool>(*opt == 42));
        *opt = 100;
        expect(static_cast<bool>(*opt == 100));
    };

    "optional_arrow_operator"_test = [] {
        luisa::optional<luisa::string> opt("hello");
        
        // operator->
        expect(static_cast<bool>(opt->size() == 5));
        opt->clear();
        expect(static_cast<bool>(opt->empty()));
    };

    "optional_value_or"_test = [] {
        luisa::optional<int> opt_val(42);
        luisa::optional<int> opt_empty;
        
        // value_or on valued optional
        expect(static_cast<bool>(opt_val.value_or(999) == 42));
        
        // value_or on empty optional
        expect(static_cast<bool>(opt_empty.value_or(999) == 999));
    };

    // ============================================
    // Modifier Tests
    // ============================================

    "optional_emplace_empty"_test = [] {
        // Emplace on empty optional
        luisa::optional<luisa::string> opt;
        opt.emplace("hello world");
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == "hello world"));
    };

    "optional_emplace_replace"_test = [] {
        // Emplace on existing value - should replace
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 10);
            expect(static_cast<bool>(TestCounter::constructor_count == 1));
            
            TestCounter::reset();
            opt.emplace(20);
            expect(static_cast<bool>(opt->value == 20));
            // Should destroy old and construct new
            expect(static_cast<bool>(TestCounter::destructor_count == 1));
            expect(static_cast<bool>(TestCounter::constructor_count == 1));
        }
    };

    "optional_emplace_multiple_args"_test = [] {
        // Emplace with multiple arguments
        luisa::optional<luisa::string> opt;
        opt.emplace(5, 'x');
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == "xxxxx"));
    };

    "optional_reset"_test = [] {
        // Reset a valued optional
        TestCounter::reset();
        {
            luisa::optional<TestCounter> opt(eastl::in_place, 42);
            expect(static_cast<bool>(opt.has_value()));
            
            opt.reset();
            expect(static_cast<bool>(!opt.has_value()));
            expect(static_cast<bool>(TestCounter::destructor_count == 1));
        }
    };

    "optional_reset_empty"_test = [] {
        // Reset an empty optional (should be safe)
        luisa::optional<int> opt;
        opt.reset();
        expect(static_cast<bool>(!opt.has_value()));
    };

    "optional_swap_both_valued"_test = [] {
        // Swap two valued optionals
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        opt1.swap(opt2);
        expect(static_cast<bool>(*opt1 == 20));
        expect(static_cast<bool>(*opt2 == 10));
    };

    "optional_swap_one_empty"_test = [] {
        // Swap with one empty
        luisa::optional<int> opt1(30);
        luisa::optional<int> opt2;
        opt1.swap(opt2);
        expect(static_cast<bool>(!opt1.has_value()));
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(*opt2 == 30));
    };

    "optional_swap_both_empty"_test = [] {
        // Swap two empty optionals
        luisa::optional<int> opt1;
        luisa::optional<int> opt2;
        opt1.swap(opt2);
        expect(static_cast<bool>(!opt1.has_value()));
        expect(static_cast<bool>(!opt2.has_value()));
    };

    "optional_global_swap"_test = [] {
        // Test global swap function
        luisa::optional<int> opt1(100);
        luisa::optional<int> opt2(200);
        swap(opt1, opt2);
        expect(static_cast<bool>(*opt1 == 200));
        expect(static_cast<bool>(*opt2 == 100));
    };

    // ============================================
    // Comparison Tests - Optional vs Optional
    // ============================================

    "optional_equality_both_valued"_test = [] {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(10);
        luisa::optional<int> opt3(20);
        
        expect(static_cast<bool>(opt1 == opt2));
        expect(static_cast<bool>(!(opt1 == opt3)));
    };

    "optional_inequality_both_valued"_test = [] {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        
        expect(static_cast<bool>(opt1 != opt2));
    };

    "optional_equality_empty"_test = [] {
        luisa::optional<int> opt1;
        luisa::optional<int> opt2;
        luisa::optional<int> opt3(10);
        
        // Empty == Empty
        expect(static_cast<bool>(opt1 == opt2));
        // Empty != Valued
        expect(static_cast<bool>(!(opt1 == opt3)));
        expect(static_cast<bool>(opt1 != opt3));
    };

    "optional_comparison_nullopt"_test = [] {
        luisa::optional<int> opt_empty;
        luisa::optional<int> opt_val(10);
        
        expect(static_cast<bool>(opt_empty == luisa::nullopt));
        expect(static_cast<bool>(luisa::nullopt == opt_empty));
        expect(static_cast<bool>(opt_val != luisa::nullopt));
        expect(static_cast<bool>(luisa::nullopt != opt_val));
    };

    "optional_ordering_both_valued"_test = [] {
        luisa::optional<int> opt1(10);
        luisa::optional<int> opt2(20);
        
        expect(static_cast<bool>(opt1 < opt2));
        expect(static_cast<bool>(opt2 > opt1));
        expect(static_cast<bool>(opt1 <= opt2));
        expect(static_cast<bool>(opt2 >= opt1));
        expect(static_cast<bool>(opt1 <= opt1));  // Equality case
        expect(static_cast<bool>(opt1 >= opt1));  // Equality case
    };

    "optional_ordering_with_empty"_test = [] {
        luisa::optional<int> opt_empty;
        luisa::optional<int> opt_val(10);
        
        // Empty < Valued
        expect(static_cast<bool>(opt_empty < opt_val));
        expect(static_cast<bool>(opt_val > opt_empty));
        
        // Empty <= Empty
        expect(static_cast<bool>(opt_empty <= opt_empty));
        expect(static_cast<bool>(opt_empty >= opt_empty));
    };

    // ============================================
    // Comparison Tests - Optional vs Value
    // ============================================

    "optional_value_equality"_test = [] {
        luisa::optional<int> opt(10);
        
        expect(static_cast<bool>(opt == 10));
        expect(static_cast<bool>(10 == opt));
        expect(static_cast<bool>(!(opt == 20)));
        expect(static_cast<bool>(opt != 20));
        expect(static_cast<bool>(20 != opt));
    };

    "optional_value_ordering"_test = [] {
        luisa::optional<int> opt(10);
        
        expect(static_cast<bool>(opt < 20));
        expect(static_cast<bool>(5 < opt));
        expect(static_cast<bool>(opt > 5));
        expect(static_cast<bool>(20 > opt));
        expect(static_cast<bool>(opt <= 10));
        expect(static_cast<bool>(opt <= 20));
        expect(static_cast<bool>(opt >= 10));
        expect(static_cast<bool>(opt >= 5));
    };

    "optional_empty_value_comparison"_test = [] {
        luisa::optional<int> opt_empty;
        
        expect(static_cast<bool>(!(opt_empty == 10)));
        expect(static_cast<bool>(!(10 == opt_empty)));
        expect(static_cast<bool>(opt_empty != 10));
        expect(static_cast<bool>(opt_empty < 10));
        expect(static_cast<bool>(!(opt_empty > 10)));
    };

    // ============================================
    // Utility Function Tests
    // ============================================

    "optional_make_optional_value"_test = [] {
        auto opt = luisa::make_optional(42);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 42));
    };

    "optional_make_optional_args"_test = [] {
        // make_optional with constructor arguments
        auto opt = luisa::make_optional<luisa::string>(luisa::string(5, 'x'));
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == "xxxxx"));
    };

    "optional_make_optional_inplace"_test = [] {
        // make_optional in-place construction
        struct Point {
            int x, y;
            Point(int _x, int _y) : x(_x), y(_y) {}
        };
        auto opt = luisa::make_optional<Point>(3, 4);
        (void)opt;
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(opt->x == 3));
        expect(static_cast<bool>(opt->y == 4));
    };

    // ============================================
    // Complex Type Tests
    // ============================================

    "optional_complex_string"_test = [] {
        // Test with string type
        luisa::optional<luisa::string> opt;
        expect(static_cast<bool>(!opt.has_value()));
        
        opt = luisa::string("hello world");
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == "hello world"));
        
        opt.reset();
        expect(static_cast<bool>(!opt.has_value()));
    };

    "optional_complex_vector"_test = [] {
        // Test with vector type
        luisa::optional<luisa::vector<int>> opt;
        expect(static_cast<bool>(!opt.has_value()));
        
        opt.emplace();
        opt->push_back(1);
        opt->push_back(2);
        opt->push_back(3);
        
        expect(static_cast<bool>(opt->size() == 3));
        expect(static_cast<bool>((*opt)[0] == 1));
        expect(static_cast<bool>((*opt)[2] == 3));
    };

    "optional_custom_class"_test = [] {
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
        expect(static_cast<bool>(!pt.has_value()));
        
        pt.emplace(3, 4);
        expect(static_cast<bool>(pt.has_value()));
        expect(static_cast<bool>(pt->x == 3));
        expect(static_cast<bool>(pt->y == 4));
    };

    // ============================================
    // Edge Cases
    // ============================================

    "optional_chained_ops"_test = [] {
        // Test chaining operations
        luisa::optional<luisa::string> opt;
        opt = luisa::string("test");
        opt->append("_string");
        expect(static_cast<bool>(*opt == "test_string"));
    };

    "optional_zero_as_value"_test = [] {
        // Test using 0 as a valid value
        luisa::optional<int> opt(0);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 0));
        expect(static_cast<bool>(opt != luisa::nullopt));
    };

    "optional_false_as_value"_test = [] {
        // Test using false as a valid value
        luisa::optional<bool> opt(false);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == false));
        expect(static_cast<bool>(opt != luisa::nullopt));
    };

    "optional_pointer_type"_test = [] {
        // Test with pointer type
        int value = 42;
        luisa::optional<int*> opt(&value);
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(**opt == 42));
    };

    "optional_move_only_type"_test = [] {
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
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(opt->value == 42));
        
        luisa::optional<MoveOnly> opt2(std::move(opt));
        expect(static_cast<bool>(opt2.has_value()));
        expect(static_cast<bool>(opt2->value == 42));
    };

    "optional_multiple_resets"_test = [] {
        // Test multiple resets
        luisa::optional<int> opt(1);
        opt.reset();
        opt.reset();
        opt.reset();
        expect(static_cast<bool>(!opt.has_value()));
    };

    "optional_reassign_after_reset"_test = [] {
        // Test reassigning after reset
        luisa::optional<int> opt(1);
        opt.reset();
        expect(static_cast<bool>(!opt.has_value()));
        
        opt = 2;
        expect(static_cast<bool>(opt.has_value()));
        expect(static_cast<bool>(*opt == 2));
        
        opt = 3;
        expect(static_cast<bool>(*opt == 3));
    };