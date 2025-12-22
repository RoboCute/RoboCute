#include "test_util.h"
#include <rbc_core/memory.h>
#include <cstring>
#include <cstdint>

// Test structures and classes
struct TrivialStruct {
    int a;
    float b;
    double c;
};

struct NonTrivialStruct {
    int value;
    static int constructor_count;
    static int destructor_count;

    NonTrivialStruct() : value(0) {
        constructor_count++;
    }

    explicit NonTrivialStruct(int v) : value(v) {
        constructor_count++;
    }

    NonTrivialStruct(int v1, int v2) : value(v1 + v2) {
        constructor_count++;
    }

    ~NonTrivialStruct() {
        destructor_count++;
    }
};

int NonTrivialStruct::constructor_count = 0;
int NonTrivialStruct::destructor_count = 0;

struct AlignedStruct {
    alignas(64) int data[16];
    int value;

    AlignedStruct() : value(0) {
        std::memset(data, 0, sizeof(data));
    }

    explicit AlignedStruct(int v) : value(v) {
        for (int i = 0; i < 16; i++) {
            data[i] = v;
        }
    }
};

TEST_SUITE("core") {
    TEST_CASE("rbc_new_delete_basic_types") {
        // Test with basic types
        int *int_ptr = RBCNew<int>(42);
        CHECK(int_ptr != nullptr);
        CHECK(*int_ptr == 42);
        RBCDelete(int_ptr);

        float *float_ptr = RBCNew<float>(3.14f);
        CHECK(float_ptr != nullptr);
        CHECK(*float_ptr == 3.14f);
        RBCDelete(float_ptr);

        double *double_ptr = RBCNew<double>(2.718);
        CHECK(double_ptr != nullptr);
        CHECK(*double_ptr == 2.718);
        RBCDelete(double_ptr);
    }

    TEST_CASE("rbc_new_delete_trivial_struct") {
        // Test with trivial struct
        TrivialStruct *ptr = RBCNew<TrivialStruct>();
        CHECK(ptr != nullptr);
        ptr->a = 10;
        ptr->b = 20.5f;
        ptr->c = 30.7;
        CHECK(ptr->a == 10);
        CHECK(ptr->b == 20.5f);
        CHECK(ptr->c == 30.7);
        RBCDelete(ptr);
    }

    TEST_CASE("rbc_new_delete_non_trivial_struct") {
        // Test with non-trivial struct (has constructor/destructor)
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>();
            CHECK(ptr != nullptr);
            CHECK(ptr->value == 0);
            CHECK(NonTrivialStruct::constructor_count == 1);
            CHECK(NonTrivialStruct::destructor_count == 0);
            RBCDelete(ptr);
        }

        CHECK(NonTrivialStruct::destructor_count == 1);
    }

    TEST_CASE("rbc_new_delete_with_args") {
        // Test constructor with arguments
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>(42);
            CHECK(ptr != nullptr);
            CHECK(ptr->value == 42);
            CHECK(NonTrivialStruct::constructor_count == 1);
            RBCDelete(ptr);
        }

        CHECK(NonTrivialStruct::destructor_count == 1);
    }

    TEST_CASE("rbc_new_delete_multiple_args") {
        // Test constructor with multiple arguments
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>(10, 20);
            CHECK(ptr != nullptr);
            CHECK(ptr->value == 30);
            CHECK(NonTrivialStruct::constructor_count == 1);
            RBCDelete(ptr);
        }

        CHECK(NonTrivialStruct::destructor_count == 1);
    }

    TEST_CASE("rbc_new_delete_nullptr") {
        // Test delete with nullptr (should not crash)
        NonTrivialStruct *ptr = nullptr;
        RBCDelete(ptr); // Should be safe
    }

    TEST_CASE("rbc_new_aligned_basic") {
        // Test aligned new/delete
        const size_t alignment = 64;
        AlignedStruct *ptr = RBCNewAligned<AlignedStruct>(alignment);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        ptr->value = 100;
        CHECK(ptr->value == 100);
        
        RBCDeleteAligned(ptr, alignment);
    }

    TEST_CASE("rbc_new_aligned_with_args") {
        // Test aligned new with constructor arguments
        const size_t alignment = 32;
        AlignedStruct *ptr = RBCNewAligned<AlignedStruct>(alignment, 42);
        CHECK(ptr != nullptr);
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        CHECK(ptr->value == 42);
        CHECK(ptr->data[0] == 42);
        
        RBCDeleteAligned(ptr, alignment);
    }

    TEST_CASE("rbc_new_aligned_various") {
        // Test various alignment values
        size_t alignments[] = {16, 32, 64, 128};
        
        for (size_t alignment : alignments) {
            int *ptr = RBCNewAligned<int>(alignment, 12345);
            CHECK(ptr != nullptr);
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            CHECK((addr % alignment) == 0);
            
            CHECK(*ptr == 12345);
            RBCDeleteAligned(ptr, alignment);
        }
    }

    TEST_CASE("rbc_new_delete_multiple") {
        // Test multiple allocations and deallocations
        const int count = 10;
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        NonTrivialStruct *ptrs[count];
        
        // Allocate
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNew<NonTrivialStruct>(i);
            CHECK(ptrs[i] != nullptr);
            CHECK(ptrs[i]->value == i);
        }
        
        CHECK(NonTrivialStruct::constructor_count == count);
        
        // Deallocate
        for (int i = 0; i < count; i++) {
            RBCDelete(ptrs[i]);
        }
        
        CHECK(NonTrivialStruct::destructor_count == count);
    }

    TEST_CASE("rbc_new_delete_aligned_multiple") {
        // Test multiple aligned allocations
        const int count = 5;
        const size_t alignment = 64;
        
        AlignedStruct *ptrs[count];
        
        // Allocate
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNewAligned<AlignedStruct>(alignment, i * 10);
            CHECK(ptrs[i] != nullptr);
            CHECK(ptrs[i]->value == i * 10);
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            CHECK((addr % alignment) == 0);
        }
        
        // Deallocate
        for (int i = 0; i < count; i++) {
            RBCDeleteAligned(ptrs[i], alignment);
        }
    }

    TEST_CASE("rbc_new_delete_n_pool_name") {
        // Test with pool name
        const char *pool_name = "test_pool";
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNewN<NonTrivialStruct>(pool_name, 99);
            CHECK(ptr != nullptr);
            CHECK(ptr->value == 99);
            CHECK(NonTrivialStruct::constructor_count == 1);
            RBCDeleteN(ptr, pool_name);
        }

        CHECK(NonTrivialStruct::destructor_count == 1);
    }

    TEST_CASE("rbc_new_delete_aligned_n_pool_name") {
        // Test aligned version with pool name
        const size_t alignment = 64;
        const char *pool_name = "aligned_pool";
        
        AlignedStruct *ptr = RBCNewAlignedN<AlignedStruct>(alignment, pool_name, 777);
        CHECK(ptr != nullptr);
        CHECK(ptr->value == 777);
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        CHECK((addr % alignment) == 0);
        
        RBCDeleteAlignedN(ptr, alignment, pool_name);
    }

    TEST_CASE("rbc_new_delete_large_object") {
        // Test with larger object
        struct LargeStruct {
            unsigned char data[1024];
            int value;
            
            LargeStruct() : value(0) {
                std::memset(data, 0, sizeof(data));
            }
            
            explicit LargeStruct(int v) : value(v) {
                std::memset(data, static_cast<unsigned char>(v), sizeof(data));
            }
        };
        
        LargeStruct *ptr = RBCNew<LargeStruct>(255);
        CHECK(ptr != nullptr);
        CHECK(ptr->value == 255);
        CHECK(ptr->data[0] == 255);
        CHECK(ptr->data[1023] == 255);
        RBCDelete(ptr);
    }

    TEST_CASE("rbc_new_delete_move_semantics") {
        // Test with move-only type
        struct MoveOnly {
            int *data;
            
            explicit MoveOnly(int value) {
                data = new int(value);
            }
            
            MoveOnly(MoveOnly &&other) noexcept : data(other.data) {
                other.data = nullptr;
            }
            
            MoveOnly &operator=(MoveOnly &&other) noexcept {
                if (this != &other) {
                    delete data;
                    data = other.data;
                    other.data = nullptr;
                }
                return *this;
            }
            
            ~MoveOnly() {
                delete data;
            }
            
            MoveOnly(const MoveOnly &) = delete;
            MoveOnly &operator=(const MoveOnly &) = delete;
        };
        
        MoveOnly *ptr = RBCNew<MoveOnly>(42);
        CHECK(ptr != nullptr);
        CHECK(ptr->data != nullptr);
        CHECK(*ptr->data == 42);
        RBCDelete(ptr);
    }

    TEST_CASE("rbc_new_delete_array_like") {
        // Test creating multiple objects manually (not array, but similar pattern)
        const int count = 5;
        int *ptrs[count];
        
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNew<int>(i * 10);
            CHECK(ptrs[i] != nullptr);
            CHECK(*ptrs[i] == i * 10);
        }
        
        for (int i = 0; i < count; i++) {
            RBCDelete(ptrs[i]);
        }
    }

    TEST_CASE("rbc_new_delete_nested") {
        // Test nested structures
        struct Outer {
            struct Inner {
                int value;
                Inner() : value(0) {}
                explicit Inner(int v) : value(v) {}
            };
            
            Inner inner;
            int outer_value;
            
            Outer() : outer_value(0) {}
            Outer(int inner_val, int outer_val) : inner(inner_val), outer_value(outer_val) {}
        };
        
        Outer *ptr = RBCNew<Outer>(100, 200);
        CHECK(ptr != nullptr);
        CHECK(ptr->inner.value == 100);
        CHECK(ptr->outer_value == 200);
        RBCDelete(ptr);
    }
}

