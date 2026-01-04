#include <rbc_core/utils/parse_string.h>
#include <rbc_core/utils/fast_float.h>
namespace rbc {
vstd::variant<int64_t, double> parse_string_to(luisa::string_view string, uint32_t base) {
    const char *start = string.data();
    const char *end = start + string.size();
    {
        int64_t result;
        auto err = std::from_chars(start, end, result, base);
        if (err.ec == std::errc{}) {
            return result;
        }
    }
    {
        double result;
        auto err = fast_float::from_chars(start, end, result);
        if (err.ec == std::errc{}) {
            return result;
        }
    }
    return {};
}
}// namespace rbc
