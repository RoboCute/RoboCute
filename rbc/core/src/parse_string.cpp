#include <rbc_core/utils/parse_string.h>
#include <rbc_core/utils/fast_float.h>
namespace rbc {
vstd::optional<int64_t> parse_string_to_int(luisa::string_view string, uint32_t base) {
    const char *start = string.data();
    const char *end = start + string.size();
    {
        int64_t result;
        auto err = std::from_chars(start, end, result, base);
        if (err.ec == std::errc{}) {
            return result;
        }
    }
    return {};
}
vstd::optional<double> parse_string_to_float(luisa::string_view string, uint32_t base) {
    const char *start = string.data();
    const char *end = start + string.size();
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
