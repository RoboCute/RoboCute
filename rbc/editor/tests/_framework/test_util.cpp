#define DOCTEST_CONFIG_IMPLEMENT
#include "test_util.h"
#include <luisa/vstl/common.h>
namespace rbc::test {

static luisa::vector<const char *> args;

inline void dt_remove(const char **argv_in) noexcept {
    args.clear();
    for (; *argv_in; ++argv_in) {
        if (!luisa::string_view{*argv_in}.starts_with("--dt-")) {
            args.emplace_back(*argv_in);
        }
    }
    args.emplace_back(nullptr);
}

int argc() noexcept {
    return static_cast<int>(args.size());
}
const char *const *argv() noexcept {
    return args.data();
}

}// namespace rbc::test

int main(int argc, const char **argv) {
    doctest::Context context(argc, argv);
    rbc::test::dt_remove(argv);
    auto test_result = context.run();
    if (context.shouldExit()) {
        return test_result;
    }
    return test_result;
}