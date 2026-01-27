#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
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