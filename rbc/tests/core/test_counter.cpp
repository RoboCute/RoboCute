#include "rbc_test.hpp"
#include <luisa/core/fiber.h>

namespace rbc::test {
suite<"Core|Counter"> CoreCounterTestSuite = [] {

    "rc"_test = [] {
        luisa::fiber::counter counter;
    };
}; // suite

} // namespace rbc::test
