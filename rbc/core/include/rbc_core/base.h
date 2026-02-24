#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/memory.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>

namespace rbc {

using RBCStruct = vstd::IOperatorNewBase;

using BasicDeserDataType = vstd::variant<
    int64_t,
    double,
    luisa::string,
    bool>;
}// namespace rbc

// Declare bin2obj embeded array
#define RBC_BIN_2_OBJ_DECLARE(VAR_NAME)                        \
    LUISA_EXTERN_C const uint8_t _binary_##VAR_NAME##_start[]; \
    LUISA_EXTERN_C const uint8_t _binary_##VAR_NAME##_end[];

// Create bin2obj embeded array's span

#define RBC_BIN_2_OBJ_SPAN(VAR_NAME) luisa::span<uint8_t const>(_binary_##VAR_NAME##_start, size_t(_binary_##VAR_NAME##_end - _binary_##VAR_NAME##_start))
