#pragma once

#include <bsdfs/polymorphic.hpp>
#include <luisa/std.hpp>

using namespace luisa::shader;

namespace integrator {

struct MediumTransition {
    uint id = max_uint32;
    bool entering;
    bool is_false = false;
    bool uses_subsurface = false;
};

}// namespace integrator
