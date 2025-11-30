#include "RBCEditor/pbrapp.h"
#include "luisa/core/dynamic_module.h"
#include "luisa/runtime/rhi/pixel.h"
#include <luisa/backends/ext/native_resource_ext.hpp>
#include "RBCEditor/base/device_config.h"

using namespace luisa;
using namespace luisa::compute;

namespace rbc {
void PBRApp::init(
    luisa::compute::Context &ctx, const char *backend_name) {
    luisa::string_view backend = backend_name;
    bool gpu_dump;
#ifdef NDEBUG
    gpu_dump = false;
#else
    gpu_dump = true;
#endif
    DeviceConfig device_config = {};

    device_config.extension = make_dx_device_config(nullptr, gpu_dump);
    device_config_ext = device_config.extension.get();
    device = ctx.create_device(backend, &device_config);
    void *native_device;
    if (backend == "dx") {
        get_dx_device(device_config_ext, native_device, dx_adaptor_luid);
    }
    stream = device.create_stream(StreamTag::GRAPHICS);
    Kernel2D clear_kernel = [](ImageFloat image) noexcept {
        image.write(dispatch_id().xy(), make_float4(0.0f));
    };
    clear_shader = device.compile(clear_kernel);
    Kernel2D draw_kernel = [](ImageFloat image, Float time, Float2 resolution) noexcept {
        auto p = dispatch_id().xy();
        auto uv = make_float2(p) / make_float2(resolution) * 2.0f - 1.0f;
        auto color = def(make_float4());
        Constant scales{pi, luisa::exp(1.f), luisa::sqrt(2.f)};
        for (auto i = 0u; i < 3u; i++) {
            color[i] = cos(time * scales[i] + uv.y * 11.f +
                           sin(-time * scales[2u - i] + uv.x * 7.f) * 4.f) *
                           .5f +
                       .5f;
        }
        color[3] = 1.0f;
        image.write(p, color);
    };
    draw_shader = device.compile(draw_kernel);
}

uint64_t PBRApp::create_texture(uint width, uint height) {
    resolution = {width, height};
    if (dummy_image && any(dummy_image.size() != uint2(width, height))) {
        cmd_list.add_callback([dummy_image = std::move(dummy_image)] {});
    }
    if (!dummy_image) {
        dummy_image = device.create_image<float>(PixelStorage::BYTE4, width, height);
    }
    // resolution changed, all textures should reload
    return (int64_t)dummy_image.native_handle();
}

void PBRApp::handle_key(luisa::compute::Key key) {
}

void PBRApp::update() {
    cmd_list << clear_shader(dummy_image).dispatch(resolution);
    float2 f_res = {(float)resolution.x, (float)resolution.y};
    cmd_list
        << draw_shader(dummy_image, clk.toc() * 1e-3, f_res).dispatch(resolution);
    stream << cmd_list.commit();
    set_dx_before_state(device_config_ext, Argument::Texture{dummy_image.handle(), 0}, D3D12EnhancedResourceUsageType::RasterRead);
}
PBRApp::~PBRApp() {
    stream.synchronize();
}

}// namespace rbc
