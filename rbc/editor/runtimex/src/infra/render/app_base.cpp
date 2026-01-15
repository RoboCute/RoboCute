#include "RBCEditorRuntime/infra/render/app_base.h"
#include "luisa/core/logging.h"
#include <rbc_graphics/make_device_config.h>
#include <luisa/backends/ext/native_resource_ext.hpp>
#include <rbc_core/state_map.h>

namespace rbc {

using namespace luisa;
using namespace luisa::compute;

void *RenderAppBase::GetStreamNativeHandle() const {
    LUISA_ASSERT(utils.present_stream());
    return utils.present_stream().native_handle();
}

void *RenderAppBase::GetDeviceNativeHandle() const {
    return RenderDevice::instance().lc_device().native_handle();
}

void RenderAppBase::process_qt_handle(QRhiNativeHandles &qt_rhi_handle) {
    if (!m_initialized) [[unlikely]] {
        LUISA_ERROR("RenderApp not Initialized when processing Qt handle");
        return;
    }

    if (m_graphicsApi == QRhi::D3D12) {
        auto &handles = static_cast<QRhiD3D12NativeHandles &>(qt_rhi_handle);
        handles.dev = GetDeviceNativeHandle();
        handles.minimumFeatureLevel = 0;
        handles.adapterLuidHigh = (qint32)GetDXAdapterLUIDHigh();
        handles.adapterLuidLow = (qint32)GetDXAdapterLUIDLow();
        handles.commandQueue = GetStreamNativeHandle();
    } else {
        LUISA_ERROR("Backend unsupported.");
    }
}

void RenderAppBase::init(const char *program_path, const char *backend_name) {
    if (m_initialized) [[unlikely]] {
        LUISA_INFO("Double Initialized RenderAppp");
        return;
    }

    if (luisa::string(backend_name) == "dx") {
        m_graphicsApi = QRhi::D3D12;
    } else {
        LUISA_ERROR("Backend unsupported.");
    }

    luisa::string_view backend = backend_name;
    ctx = luisa::make_unique<luisa::compute::Context>(program_path);

    void *native_device;
    utils.init_device(program_path, backend);

    auto &render_device = RenderDevice::instance();
    get_dx_device(render_device.lc_device_ext(), native_device, dx_adapter_luid);

    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() /
        (luisa::string("shader_build_") + utils.backend_name()));
    utils.init_render();
    pipe_ctx = utils.register_render_pipectx({});
    auto *cam = &utils.render_settings(pipe_ctx).read_mut<Camera>();
    cam->fov = radians(80.f);
    cam_controller.camera = cam;
    last_frame_time = clk.toc();
    // 调用子类钩子
    on_init();
    m_initialized = true;
}

uint64_t RenderAppBase::get_present_texture(uint width, uint height) {
    resolution = {width, height};
    if (utils.dst_image() && any(resolution != utils.dst_image().size())) {
        utils.resize_swapchain(resolution, 0, invalid_resource_handle);
    }
    if (!utils.dst_image()) {
        utils.init_display(resolution, 0, invalid_resource_handle);
    }
    return (uint64_t)utils.dst_image().native_handle();
}

void RenderAppBase::handle_reset() {
    if (reset) {
        reset = false;
        utils.reset_frame();
    }
}

void RenderAppBase::prepare_dx_states() {
    auto &render_device = RenderDevice::instance();
    clear_dx_states(render_device.lc_device_ext());
    add_dx_before_state(
        render_device.lc_device_ext(),
        Argument::Texture{utils.dst_image().handle(), 0},
        D3D12EnhancedResourceUsageType::RasterRead);
}

void RenderAppBase::update_camera(float delta_time) {
    auto &cam = utils.render_settings(pipe_ctx).read_mut<Camera>();
    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    camera_input.viewport_size = {(float)(resolution.x), (float)(resolution.y)};

    cam_controller.grab_input_from_viewport(camera_input, delta_time);
    if (cam_controller.any_changed()) {
        frame_index = 0;
    }
}

void RenderAppBase::dispose() {
    utils.dispose([&]() {
        auto pipe_settings_json = utils.render_settings(pipe_ctx).serialize_to_json();
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

RenderAppBase::~RenderAppBase() {
    // dispose() 应由子类调用，但作为安全保障
}

}// namespace rbc
