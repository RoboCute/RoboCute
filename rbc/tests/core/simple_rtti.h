#pragma once
#include <luisa/vstl/common.h>
#include <rbc_core/type_info.h>

struct Dummy {
    int a = 1;
    float b = 1.2f;
};

RBC_RTTI(Dummy);