#include "test_util.h"
#include <luisa/vstl/vector.h>
#include <luisa/core/stl/algorithm.h>

TEST_SUITE("core") {
    TEST_CASE("basic") {
        luisa::vector<int> a{3, 5, 6, 9, 1};
        luisa::vector<int> sorted_a = {1, 3, 5, 6, 9};
        luisa::sort(a.begin(), a.end());
        for (auto i = 0; i < a.size(); i++) {
            CHECK(a[i] == sorted_a[i]);
        }
    }
}