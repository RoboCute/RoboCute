#pragma once
#include <rbc_config.h>
#include <luisa/vstl/meta_lib.h>
namespace rbc {
RBC_CORE_API vstd::optional<int64_t> parse_string_to_int(luisa::string_view string, uint32_t base = 10);
RBC_CORE_API vstd::optional<double> parse_string_to_float(luisa::string_view string, uint32_t base = 10);
}// namespace rbc