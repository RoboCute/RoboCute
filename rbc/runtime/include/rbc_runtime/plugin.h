#pragma once
#include <luisa/vstl/meta_lib.h>

namespace rbc {
struct Plugin : vstd::IOperatorNewBase {
protected:
    ~Plugin() = default;
};
}// namespace rbc