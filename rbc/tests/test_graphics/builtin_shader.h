#pragma once
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/mathematics.h>
#include <rbc_graphics/render_device.h>
#include <rbc_core/rc.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
namespace detail {
template<typename T>
struct TextureType {
    using Type = T;
};
template<>
struct TextureType<half> {
    using Type = float;
};
template<>
struct TextureType<uint16_t> {
    using Type = uint;
};
// Helper to check if a type is a signed integer
template<typename T>
struct IsSignedInt {
    static constexpr bool value = std::is_signed_v<T> && std::is_integral_v<T>;
};
}// namespace detail
struct BuiltinShaders : RCBase {
private:
    // image_to_buffer shaders
    Shader2D<Image<float>, Buffer<half>, uint, uint2> const *_image_to_buffer_f16{nullptr};
    Shader2D<Image<float>, Buffer<float>, uint, uint2> const *_image_to_buffer_f32{nullptr};
    Shader2D<Image<uint>, Buffer<uint16_t>, uint, uint2> const *_image_to_buffer_u16{nullptr};
    Shader2D<Image<uint>, Buffer<uint>, uint, uint2> const *_image_to_buffer_u32{nullptr};
    // buffer_to_image shaders
    Shader2D<Buffer<half>, Image<float>, uint, uint2> const *_buffer_to_image_f16{nullptr};
    Shader2D<Buffer<float>, Image<float>, uint, uint2> const *_buffer_to_image_f32{nullptr};
    Shader2D<Buffer<uint16_t>, Image<uint>, uint, uint2> const *_buffer_to_image_u16{nullptr};
    Shader2D<Buffer<uint>, Image<uint>, uint, uint2> const *_buffer_to_image_u32{nullptr};


public:
    // Helper function to compact uint4 swizzle to uint swizzle_bytes
    // Each byte represents one output channel's source channel (0-3 for RGBA, 255 for invalid)
    static uint compact_swizzle(uint4 swizzle) {
        swizzle = clamp(swizzle, 0u, 3u);
        return (swizzle.x & 0xFF) |
               ((swizzle.y & 0xFF) << 8) |
               ((swizzle.z & 0xFF) << 16) |
               ((swizzle.w & 0xFF) << 24);
    }

    BuiltinShaders();
    ~BuiltinShaders();

    // Main dispatch function for standard types
    template<typename T>
    void dispatch_image_to_buffer(
        ImageView<typename detail::TextureType<T>::Type> input_texture,
        BufferView<T> output_buffer,
        uint2 pixel_offset,
        uint2 pixel_size,
        uint4 swizzle) {

        uint swizzle_bytes = compact_swizzle(swizzle);
        auto &cmdlist = RenderDevice::instance().lc_main_cmd_list();
        auto dispatch_size = min(input_texture.size() - pixel_offset, pixel_size);
        if constexpr (std::is_same_v<T, half>) {
            cmdlist << (*_image_to_buffer_f16)(input_texture, output_buffer, swizzle_bytes, pixel_offset).dispatch(dispatch_size);
        } else if constexpr (std::is_same_v<T, float>) {
            cmdlist << (*_image_to_buffer_f32)(input_texture, output_buffer, swizzle_bytes, pixel_offset).dispatch(dispatch_size);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            cmdlist << (*_image_to_buffer_u16)(input_texture, output_buffer, swizzle_bytes, pixel_offset).dispatch(dispatch_size);
        } else if constexpr (std::is_same_v<T, uint>) {
            cmdlist << (*_image_to_buffer_u32)(input_texture, output_buffer, swizzle_bytes, pixel_offset).dispatch(dispatch_size);
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid type.");
        }
    }
    // Main dispatch function for buffer_to_image
    template<typename T>
    void dispatch_buffer_to_image(
        BufferView<T> input_buffer,
        ImageView<typename detail::TextureType<T>::Type> output_texture,
        uint2 pixel_offset,
        uint2 pixel_size,
        uint4 swizzle) {

        uint swizzle_bytes = compact_swizzle(swizzle);
        auto &cmdlist = RenderDevice::instance().lc_main_cmd_list();
        if constexpr (std::is_same_v<T, half>) {
            cmdlist << (*_buffer_to_image_f16)(input_buffer, output_texture, swizzle_bytes, pixel_offset).dispatch(pixel_size);
        } else if constexpr (std::is_same_v<T, float>) {
            cmdlist << (*_buffer_to_image_f32)(input_buffer, output_texture, swizzle_bytes, pixel_offset).dispatch(pixel_size);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            cmdlist << (*_buffer_to_image_u16)(input_buffer, output_texture, swizzle_bytes, pixel_offset).dispatch(pixel_size);
        } else if constexpr (std::is_same_v<T, uint>) {
            cmdlist << (*_buffer_to_image_u32)(input_buffer, output_texture, swizzle_bytes, pixel_offset).dispatch(pixel_size);
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid type.");
        }
    }
};
}// namespace rbc