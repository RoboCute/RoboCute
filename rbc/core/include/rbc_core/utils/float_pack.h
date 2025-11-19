#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/mathematics.h>
namespace rbc
{
using namespace luisa;
static uint float_pack_to_uint(float val)
{
    uint uvalue = reinterpret_cast<uint&>(val);
    return select(~uvalue, uvalue | (1u << 31u), uvalue >> 31u == 0u);
}

static float uint_unpack_to_float(uint val)
{
    uint uvalue = select(val & (~(1u << 31u)), ~val, val >> 31u == 0);
    return reinterpret_cast<float&>(uvalue);
}
} // namespace rbc