#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
#endif
/// Gaussian probe data structure
struct GaussianProbe {
    float4 rotation;// quaternion
    std::array<float, 3> position;
    float opacity;
    std::array<float, 3> scale;
};