#pragma once
#include <luisa/vstl/meta_lib.h>

namespace rbc {
struct Plugin : vstd::IOperatorNewBase {
    virtual ~Plugin() = default;
};
}// namespace rbc