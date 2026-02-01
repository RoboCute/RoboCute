#pragma once
#include <luisa/runtime/rhi/resource.h>
struct BufferCreationInfoInterop : luisa::compute::BufferCreationInfo {
    bool interop{false};
};

struct TextureCreationInfo : luisa::compute::ResourceCreationInfo {
    luisa::compute::PixelFormat format;
    uint32_t dimension;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipmap_levels;
};