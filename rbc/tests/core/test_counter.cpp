#include "test_util.h"
#include <luisa/core/fiber.h>

TEST_SUITE("core") {
    TEST_CASE("rc") {
        luisa::fiber::counter counter;
    }
}