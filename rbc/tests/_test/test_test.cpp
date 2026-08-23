#include <luisa/vstl/common.h>
#include "rbc_test.hpp"

namespace rbc::test {
suite SampleSuite = [] {
    "hello"_test = [] {
        expect(true);
    };
};
}// namespace rbc::test