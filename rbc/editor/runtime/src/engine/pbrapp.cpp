#include "RBCEditorRuntime/engine/pbrapp.h"
#include "luisa/core/dynamic_module.h"
#include "luisa/runtime/rhi/pixel.h"
#include <rbc_graphics/make_device_config.h>
#include <luisa/backends/ext/native_resource_ext.hpp>

using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void PBRApp::init(
    const char *program_path, const char *backend_name) {
    luisa::string_view backend = backend_name;
    bool gpu_dump;
    ctx = luisa::make_unique<luisa::compute::Context>(program_path);
#ifdef NDEBUG
    gpu_dump = false;
#else
    gpu_dump = true;
#endif

    void *native_device;
    utils.init_device(
        program_path,
        backend);

    auto &render_device = RenderDevice::instance();
    get_dx_device(render_device.lc_device_ext(), native_device, dx_adaptor_luid);

    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name()));
    utils.init_render();

    utils.render_plugin()->update_skybox("../sky.bytes", uint2(4096, 2048));
    simple_scene.create(*Lights::instance());
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    cam.fov = radians(80.f);
}

uint64_t PBRApp::create_texture(uint width, uint height) {
    resolution = {width, height};
    if (utils.dst_image() && any(resolution != utils.dst_image().size())) {
        utils.resize_swapchain(resolution);
    }
    if (!utils.dst_image()) {
        utils.init_display(resolution);
    }
    return (uint64_t)utils.dst_image().native_handle();
}

void PBRApp::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    frame_index = 0;
    reset = true;
    switch (key) {
        case Key::KEY_SPACE: {
            LUISA_INFO("Reset frame");
        } break;
        case Key::KEY_W: {
            light_move.create();
            *light_move += float3(0, 0.1, 0);
        } break;
        case Key::KEY_S: {
            light_move.create();
            *light_move += float3(0, -0.1, 0);
        } break;
        case Key::KEY_A: {
            light_move.create();
            *light_move += float3(-0.1, 0, 0);
        } break;
        case Key::KEY_D: {
            light_move.create();
            *light_move += float3(0.1, 0, 0);
        } break;
        case Key::KEY_Q: {
            light_move.create();
            *light_move += float3(0, 0, -0.1);
        } break;
        case Key::KEY_E: {
            light_move.create();
            *light_move += float3(0, 0, 0.1);
        } break;
        case Key::KEY_UP: {
            cube_move.create();
            *cube_move += float3(0, 0.1, 0);
        } break;
        case Key::KEY_DOWN: {
            cube_move.create();
            *cube_move += float3(0, -0.1, 0);
        } break;
        case Key::KEY_LEFT: {
            cube_move.create();
            *cube_move += float3(-0.1, 0, 0);
        } break;
        case Key::KEY_RIGHT: {
            cube_move.create();
            *cube_move += float3(0.1, 0, 0);
        } break;
    }
}

void PBRApp::update() {
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    if (reset) {
        reset = false;
        utils.reset_frame();
    }
    auto &render_device = RenderDevice::instance();

    clear_dx_states(render_device.lc_device_ext());
    add_dx_before_state(render_device.lc_device_ext(), Argument::Texture{utils.dst_image().handle(), 0}, D3D12EnhancedResourceUsageType::RasterRead);
    add_dx_after_state(render_device.lc_device_ext(), Argument::Texture{utils.dst_image().handle(), 0}, D3D12EnhancedResourceUsageType::RasterRead);

    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    auto time = clk.toc();
    auto delta_time = time - last_frame_time;
    last_frame_time = time;
    // scene logic
    if (cube_move) {
        simple_scene->move_cube(*cube_move);
        cube_move.destroy();
    }
    if (light_move) {
        simple_scene->move_light(*light_move);
        light_move.destroy();
    }
    utils.tick(
        (float)delta_time,
        frame_index,
        resolution,
        GraphicsUtils::TickStage::PresentOfflineResult);

    ++frame_index;
}
PBRApp::~PBRApp() {
    utils.dispose([&]() {
        auto pipe_settings_json = utils.render_settings().serialize_to_json();
        if (pipe_settings_json.data()) {
            LUISA_INFO(
                "{}",
                luisa::string_view{
                    (char const *)pipe_settings_json.data(),
                    pipe_settings_json.size()});
        }
        // destroy render-pipeline
        simple_scene.destroy();
    });
    ctx.reset();
}
void *PBRApp::GetStreamNativeHandle() const {
    LUISA_ASSERT(utils.present_stream());
    return utils.present_stream().native_handle();
}
}// namespace rbc
