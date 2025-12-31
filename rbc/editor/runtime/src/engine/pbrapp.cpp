#include "RBCEditorRuntime/engine/pbrapp.h"
#include "luisa/core/dynamic_module.h"
#include "luisa/runtime/rhi/pixel.h"
#include <rbc_graphics/make_device_config.h>
#include <luisa/backends/ext/native_resource_ext.hpp>

using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void PBRApp::on_init() {
    simple_scene.create(*Lights::instance());
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
        default:
            break;
    }
}

void PBRApp::update() {
    handle_reset();

    auto &render_device = RenderDevice::instance();
    clear_dx_states(render_device.lc_device_ext());
    add_dx_before_state(
        render_device.lc_device_ext(), 
        Argument::Texture{utils.dst_image().handle(), 0}, 
        D3D12EnhancedResourceUsageType::RasterRead);
    add_dx_after_state(
        render_device.lc_device_ext(), 
        Argument::Texture{utils.dst_image().handle(), 0}, 
        D3D12EnhancedResourceUsageType::RasterRead);

    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
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

}// namespace rbc
