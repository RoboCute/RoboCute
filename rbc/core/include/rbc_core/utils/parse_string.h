#pragma once
#include <rbc_config.h>
#include <luisa/vstl/meta_lib.h>
namespace rbc {
RBC_CORE_API vstd::variant<int64_t, double> parse_string_to(luisa::string_view string, uint32_t base = 10);
}// namespace rbc