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

    "rbc_new_delete_basic_types"_test = [] {
        // Test with basic types
        int *int_ptr = RBCNew<int>(42);
        expect(static_cast<bool>(int_ptr != nullptr));
        expect(static_cast<bool>(*int_ptr == 42));
        RBCDelete(int_ptr);

        float *float_ptr = RBCNew<float>(3.14f);
        expect(static_cast<bool>(float_ptr != nullptr));
        expect(static_cast<bool>(*float_ptr == 3.14f));
        RBCDelete(float_ptr);

        double *double_ptr = RBCNew<double>(2.718);
        expect(static_cast<bool>(double_ptr != nullptr));
        expect(static_cast<bool>(*double_ptr == 2.718));
        RBCDelete(double_ptr);
    };

    "rbc_new_delete_trivial_struct"_test = [] {
        // Test with trivial struct
        TrivialStruct *ptr = RBCNew<TrivialStruct>();
        expect(static_cast<bool>(ptr != nullptr));
        ptr->a = 10;
        ptr->b = 20.5f;
        ptr->c = 30.7;
        expect(static_cast<bool>(ptr->a == 10));
        expect(static_cast<bool>(ptr->b == 20.5f));
        expect(static_cast<bool>(ptr->c == 30.7));
        RBCDelete(ptr);
    };

    "rbc_new_delete_non_trivial_struct"_test = [] {
        // Test with non-trivial struct (has constructor/destructor)
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>();
            expect(static_cast<bool>(ptr != nullptr));
            expect(static_cast<bool>(ptr->value == 0));
            expect(static_cast<bool>(NonTrivialStruct::constructor_count == 1));
            expect(static_cast<bool>(NonTrivialStruct::destructor_count == 0));
            RBCDelete(ptr);
        }

        expect(static_cast<bool>(NonTrivialStruct::destructor_count == 1));
    };

    "rbc_new_delete_with_args"_test = [] {
        // Test constructor with arguments
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>(42);
            expect(static_cast<bool>(ptr != nullptr));
            expect(static_cast<bool>(ptr->value == 42));
            expect(static_cast<bool>(NonTrivialStruct::constructor_count == 1));
            RBCDelete(ptr);
        }

        expect(static_cast<bool>(NonTrivialStruct::destructor_count == 1));
    };

    "rbc_new_delete_multiple_args"_test = [] {
        // Test constructor with multiple arguments
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNew<NonTrivialStruct>(10, 20);
            expect(static_cast<bool>(ptr != nullptr));
            expect(static_cast<bool>(ptr->value == 30));
            expect(static_cast<bool>(NonTrivialStruct::constructor_count == 1));
            RBCDelete(ptr);
        }

        expect(static_cast<bool>(NonTrivialStruct::destructor_count == 1));
    };

    "rbc_new_delete_nullptr"_test = [] {
        // Test delete with nullptr (should not crash)
        NonTrivialStruct *ptr = nullptr;
        RBCDelete(ptr); // Should be safe
    };

    "rbc_new_aligned_basic"_test = [] {
        // Test aligned new/delete
        const size_t alignment = 64;
        AlignedStruct *ptr = RBCNewAligned<AlignedStruct>(alignment);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        ptr->value = 100;
        expect(static_cast<bool>(ptr->value == 100));
        
        RBCDeleteAligned(ptr, alignment);
    };

    "rbc_new_aligned_with_args"_test = [] {
        // Test aligned new with constructor arguments
        const size_t alignment = 32;
        AlignedStruct *ptr = RBCNewAligned<AlignedStruct>(alignment, 42);
        expect(static_cast<bool>(ptr != nullptr));
        
        // Check alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        expect(static_cast<bool>(ptr->value == 42));
        expect(static_cast<bool>(ptr->data[0] == 42));
        
        RBCDeleteAligned(ptr, alignment);
    };

    "rbc_new_aligned_various"_test = [] {
        // Test various alignment values
        size_t alignments[] = {16, 32, 64, 128};
        
        for (size_t alignment : alignments) {
            int *ptr = RBCNewAligned<int>(alignment, 12345);
            expect(static_cast<bool>(ptr != nullptr));
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            expect(static_cast<bool>((addr % alignment) == 0));
            
            expect(static_cast<bool>(*ptr == 12345));
            RBCDeleteAligned(ptr, alignment);
        }
    };

    "rbc_new_delete_multiple"_test = [] {
        // Test multiple allocations and deallocations
        const int count = 10;
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        NonTrivialStruct *ptrs[count];
        
        // Allocate
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNew<NonTrivialStruct>(i);
            expect(static_cast<bool>(ptrs[i] != nullptr));
            expect(static_cast<bool>(ptrs[i]->value == i));
        }
        
        expect(static_cast<bool>(NonTrivialStruct::constructor_count == count));
        
        // Deallocate
        for (int i = 0; i < count; i++) {
            RBCDelete(ptrs[i]);
        }
        
        expect(static_cast<bool>(NonTrivialStruct::destructor_count == count));
    };

    "rbc_new_delete_aligned_multiple"_test = [] {
        // Test multiple aligned allocations
        const int count = 5;
        const size_t alignment = 64;
        
        AlignedStruct *ptrs[count];
        
        // Allocate
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNewAligned<AlignedStruct>(alignment, i * 10);
            expect(static_cast<bool>(ptrs[i] != nullptr));
            expect(static_cast<bool>(ptrs[i]->value == i * 10));
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            expect(static_cast<bool>((addr % alignment) == 0));
        }
        
        // Deallocate
        for (int i = 0; i < count; i++) {
            RBCDeleteAligned(ptrs[i], alignment);
        }
    };

    "rbc_new_delete_n_pool_name"_test = [] {
        // Test with pool name
        const char *pool_name = "test_pool";
        NonTrivialStruct::constructor_count = 0;
        NonTrivialStruct::destructor_count = 0;

        {
            NonTrivialStruct *ptr = RBCNewN<NonTrivialStruct>(pool_name, 99);
            expect(static_cast<bool>(ptr != nullptr));
            expect(static_cast<bool>(ptr->value == 99));
            expect(static_cast<bool>(NonTrivialStruct::constructor_count == 1));
            RBCDeleteN(ptr, pool_name);
        }

        expect(static_cast<bool>(NonTrivialStruct::destructor_count == 1));
    };

    "rbc_new_delete_aligned_n_pool_name"_test = [] {
        // Test aligned version with pool name
        const size_t alignment = 64;
        const char *pool_name = "aligned_pool";
        
        AlignedStruct *ptr = RBCNewAlignedN<AlignedStruct>(alignment, pool_name, 777);
        expect(static_cast<bool>(ptr != nullptr));
        expect(static_cast<bool>(ptr->value == 777));
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        expect(static_cast<bool>((addr % alignment) == 0));
        
        RBCDeleteAlignedN(ptr, alignment, pool_name);
    };

    "rbc_new_delete_large_object"_test = [] {
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
        expect(static_cast<bool>(ptr != nullptr));
        expect(static_cast<bool>(ptr->value == 255));
        expect(static_cast<bool>(ptr->data[0] == 255));
        expect(static_cast<bool>(ptr->data[1023] == 255));
        RBCDelete(ptr);
    };

    "rbc_new_delete_move_semantics"_test = [] {
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
        expect(static_cast<bool>(ptr != nullptr));
        expect(static_cast<bool>(ptr->data != nullptr));
        expect(static_cast<bool>(*ptr->data == 42));
        RBCDelete(ptr);
    };

    "rbc_new_delete_array_like"_test = [] {
        // Test creating multiple objects manually (not array, but similar pattern)
        const int count = 5;
        int *ptrs[count];
        
        for (int i = 0; i < count; i++) {
            ptrs[i] = RBCNew<int>(i * 10);
            expect(static_cast<bool>(ptrs[i] != nullptr));
            expect(static_cast<bool>(*ptrs[i] == i * 10));
        }
        
        for (int i = 0; i < count; i++) {
            RBCDelete(ptrs[i]);
        }
    };

    "rbc_new_delete_nested"_test = [] {
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
        expect(static_cast<bool>(ptr != nullptr));
        expect(static_cast<bool>(ptr->inner.value == 100));
        expect(static_cast<bool>(ptr->outer_value == 200));
        RBCDelete(ptr);
    };
