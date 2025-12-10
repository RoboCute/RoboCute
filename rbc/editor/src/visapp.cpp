#include "RBCEditor/VisApp.h"
#include "luisa/core/dynamic_module.h"
#include "luisa/core/logging.h"
#include "luisa/runtime/rhi/pixel.h"
#include <rbc_graphics/make_device_config.h>
#include <rbc_render/click_manager.h>
#include <luisa/backends/ext/native_resource_ext.hpp>

using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void VisApp::init(
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

    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    cam.fov = radians(80.f);
    cam_controller.camera = &cam;
    last_frame_time = clk.toc();
}

uint64_t VisApp::create_texture(uint width, uint height) {
    resolution = {width, height};

    if (utils.dst_image() && any(resolution != utils.dst_image().size())) {
        utils.resize_swapchain(resolution);
        dst_image_reseted = true;
    }
    if (!utils.dst_image()) {
        utils.init_display(resolution);
        dst_image_reseted = true;
    }
    return (uint64_t)utils.dst_image().native_handle();
}

void VisApp::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    bool pressed = false;
    if (action == Action::ACTION_PRESSED) {
        pressed = true;
    } else if (action == Action::ACTION_RELEASED) {
        pressed = false;
    } else {
        return;
    }
    switch (key) {
        case Key::KEY_SPACE: {
            camera_input.is_space_down = pressed;
        } break;
        case Key::KEY_RIGHT_SHIFT:
        case Key::KEY_LEFT_SHIFT: {
            camera_input.is_shift_down = pressed;
        } break;
        case Key::KEY_W: {
            camera_input.is_front_dir_key_pressed = pressed;
        } break;
        case Key::KEY_S: {
            camera_input.is_back_dir_key_pressed = pressed;
        } break;
        case Key::KEY_A: {
            camera_input.is_left_dir_key_pressed = pressed;
        } break;
        case Key::KEY_D: {
            camera_input.is_right_dir_key_pressed = pressed;
        } break;
        case Key::KEY_Q: {
            camera_input.is_up_dir_key_pressed = pressed;
        } break;
        case Key::KEY_E: {
            camera_input.is_down_dir_key_pressed = pressed;
        } break;
    }
}

void VisApp::handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) {
    if (button == MOUSE_BUTTON_LEFT) {
        if (action == Action::ACTION_PRESSED) {
            start_uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));
            end_uv = start_uv;
            mouse_stage = MouseStage::Dragging;
        } else if (action == Action::ACTION_RELEASED) {
            if (mouse_stage == MouseStage::Dragging && length(abs(start_uv - end_uv)) > 1e-2f)
                mouse_stage = MouseStage::None;
            else {
                start_uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));
                mouse_stage = MouseStage::Clicking;
            }
        }
    } else if (button == MOUSE_BUTTON_RIGHT) {
        if (action == Action::ACTION_PRESSED) {
            camera_input.is_mouse_right_down = true;
        } else if (action == Action::ACTION_RELEASED) {
            camera_input.is_mouse_right_down = false;
        }
    }
}
void VisApp::handle_cursor_position(luisa::float2 xy) {
    if (mouse_stage == MouseStage::Dragging) {
        end_uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));
    }
    camera_input.mouse_cursor_pos = xy;
}

void VisApp::update() {

    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    auto &click_mng = utils.render_settings().read_mut<ClickManager>();

    if (reset) {
        reset = false;
        utils.reset_frame();
    }
    auto &render_device = RenderDevice::instance();

    clear_dx_states(render_device.lc_device_ext());
    add_dx_before_state(render_device.lc_device_ext(), Argument::Texture{utils.dst_image().handle(), 0}, D3D12EnhancedResourceUsageType::RasterRead);

    dst_image_reseted = false;
    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    auto time = clk.toc();

    auto delta_time = time - last_frame_time;
    last_frame_time = time;

    // handle camera control
    cam_controller.grab_input_from_viewport(camera_input, delta_time);
    if (cam_controller.any_changed())
        frame_index = 0;

    // click and drag
    if (mouse_stage == MouseStage::Clicking) {
        click_mng.add_require("click", ClickRequire{.screen_uv = start_uv});
    }
    // set drag
    else if (mouse_stage == MouseStage::Dragging) {
        click_mng.add_frame_selection("dragging", min(start_uv, end_uv) * 2.f - 1.f, max(start_uv, end_uv) * 2.f - 1.f, true);
    }

    if (mouse_stage == MouseStage::Clicking || mouse_stage == MouseStage::Dragging) {
        dragged_object_ids.clear();
    }
    if (mouse_stage == MouseStage::Clicked) {
        auto click_result = click_mng.query_result("click");
        if (click_result) {
            dragged_object_ids.push_back(click_result->inst_id);
        }
    } else if (mouse_stage == MouseStage::Dragging) {
        auto dragging_result = click_mng.query_frame_selection("dragging");
        if (!dragging_result.empty())
            dragged_object_ids = std::move(dragging_result);
    }
    auto tick_stage = GraphicsUtils::TickStage::PathTracingPreview;
    click_mng.set_contour_objects(luisa::vector<uint>{dragged_object_ids});

    utils.tick(
        (float)delta_time,
        frame_index,
        resolution,
        GraphicsUtils::TickStage::RasterPreview);
}
VisApp::~VisApp() {
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
    });
    ctx.reset();
}

}// namespace rbc
