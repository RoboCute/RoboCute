#include "RBCEditorRuntime/engine/app_base.h"
#include "luisa/core/logging.h"
#include <rbc_graphics/make_device_config.h>
#include <luisa/backends/ext/native_resource_ext.hpp>

using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void *AppBase::GetStreamNativeHandle() const {
    LUISA_ASSERT(utils.present_stream());
    return utils.present_stream().native_handle();
}

void *AppBase::GetDeviceNativeHandle() const {
    return RenderDevice::instance().lc_device().native_handle();
}

void AppBase::init(const char *program_path, const char *backend_name) {
    luisa::string_view backend = backend_name;
    ctx = luisa::make_unique<luisa::compute::Context>(program_path);

    void *native_device;
    utils.init_device(program_path, backend);

    auto &render_device = RenderDevice::instance();
    get_dx_device(render_device.lc_device_ext(), native_device, dx_adaptor_luid);

    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / 
        (luisa::string("shader_build_") + utils.backend_name()));
    utils.init_render();

    utils.render_plugin()->update_skybox("../sky.bytes", PixelStorage::FLOAT4, uint2(4096, 2048));

    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    cam.fov = radians(80.f);
    cam_controller.camera = &cam;

    last_frame_time = clk.toc();

    // 调用子类钩子
    on_init();
}

uint64_t AppBase::create_texture(uint width, uint height) {
    resolution = {width, height};

    if (utils.dst_image() && any(resolution != utils.dst_image().size())) {
        utils.resize_swapchain(resolution);
    }
    if (!utils.dst_image()) {
        utils.init_display(resolution);
    }
    return (uint64_t)utils.dst_image().native_handle();
}

void AppBase::handle_reset() {
    if (reset) {
        reset = false;
        utils.reset_frame();
    }
}

void AppBase::prepare_dx_states() {
    auto &render_device = RenderDevice::instance();
    clear_dx_states(render_device.lc_device_ext());
    add_dx_before_state(
        render_device.lc_device_ext(), 
        Argument::Texture{utils.dst_image().handle(), 0}, 
        D3D12EnhancedResourceUsageType::RasterRead);
}

void AppBase::update_camera(float delta_time) {
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    camera_input.viewport_size = {(float)(resolution.x), (float)(resolution.y)};

    cam_controller.grab_input_from_viewport(camera_input, delta_time);
    if (cam_controller.any_changed()) {
        frame_index = 0;
    }
}

void AppBase::dispose() {
    utils.dispose([&]() {
        auto pipe_settings_json = utils.render_settings().serialize_to_json();
        if (pipe_settings_json.data()) {
            LUISA_INFO(
                "{}",
                luisa::string_view{
                    (char const *)pipe_settings_json.data(),
                    pipe_settings_json.size()});
        }
    });
    ctx.reset();
}

AppBase::~AppBase() {
    // dispose() 应由子类调用，但作为安全保障
}

}// namespace rbc

