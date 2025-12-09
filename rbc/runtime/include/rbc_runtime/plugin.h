#pragma once
#include <luisa/vstl/meta_lib.h>

namespace rbc {
struct Plugin {
    virtual void dispose() = 0;
    virtual ~Plugin() = default;
};
}// namespace rbc