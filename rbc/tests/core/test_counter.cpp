#include "test_util.h"
#include <luisa/core/fiber.h>

    "rc"_test = [] {
        luisa::fiber::counter counter;
    };