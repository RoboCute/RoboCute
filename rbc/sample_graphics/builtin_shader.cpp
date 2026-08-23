#include "builtin_shader.h"
#include <luisa/core/fiber.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {
BuiltinShaders::BuiltinShaders() {
    // Load all builtin shaders
    luisa::fiber::counter counter;
    auto isnt = ShaderManager::instance();
    isnt->async_load(counter, "builtin/image_to_buffer_f16.bin", _image_to_buffer_f16);
    isnt->async_load(counter, "builtin/image_to_buffer_f32.bin", _image_to_buffer_f32);
    isnt->async_load(counter, "builtin/image_to_buffer_u16.bin", _image_to_buffer_u16);
    isnt->async_load(counter, "builtin/image_to_buffer_u32.bin", _image_to_buffer_u32);
    isnt->async_load(counter, "builtin/buffer_to_image_f16.bin", _buffer_to_image_f16);
    isnt->async_load(counter, "builtin/buffer_to_image_f32.bin", _buffer_to_image_f32);
    isnt->async_load(counter, "builtin/buffer_to_image_u16.bin", _buffer_to_image_u16);
    isnt->async_load(counter, "builtin/buffer_to_image_u32.bin", _buffer_to_image_u32);
    counter.wait();
}
BuiltinShaders::~BuiltinShaders() {}
}// namespace rbc