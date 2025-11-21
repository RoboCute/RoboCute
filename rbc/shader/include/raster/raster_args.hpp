#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace raster {
using namespace luisa::shader;
}// namespace raster
#else
namespace raster {
using namespace luisa;
}// namespace raster
#endif

namespace raster {
struct VertArgs {
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
};
struct PixelArgs {
    float3 cam_pos;
    uint vt_frame_countdown;
};
}