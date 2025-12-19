#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include "simple_scene.h"
#include "world_scene.h"
#include <rbc_app/graphics_utils.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
#include <rbc_app/camera_controller.h>
#include <rbc_runtime/plugin_manager.h>
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>
#ifdef STANDALONE
int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }
    GraphicsUtils utils;
    PluginManager::init();
    utils.init_device(
        argv[0],
        backend.c_str());
    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name()));
    utils.init_render();
    utils.render_plugin()->update_skybox("../sky.bytes", PixelStorage::FLOAT4, uint2(4096, 2048));
    utils.init_display_with_window(luisa::string{"test_graphics_"} + utils.backend_name(), uint2(1024), true);
    uint64_t frame_index = 0;
    // Present is ping-pong frame-buffer and compute is triple-buffer
    Clock clk;
    double last_frame_time = 0;
    // vstd::optional<SimpleScene> simple_scene;
    vstd::optional<WorldScene> world_scene;
    world_scene.create(&utils);
    // simple_scene.create(*Lights::instance());
    // Test FOV
    bool reset = false;
    auto &click_mng = utils.render_settings().read_mut<ClickManager>();
    uint2 window_size = utils.window()->size();
    float2 start_uv, end_uv;
    luisa::vector<uint> dragged_object_ids;
    uint clicked_user_id;
    enum struct MouseStage {
        None,
        Dragging,
        Clicking,
        Clicked
    };

    MouseStage stage{MouseStage::None};
    CameraController::Input camera_input;
    utils.window()->set_mouse_callback([&](MouseButton button, Action action, float2 xy) {
        if (button == MOUSE_BUTTON_1) {
            if (action == Action::ACTION_PRESSED) {
                start_uv = clamp(xy / make_float2(window_size), float2(0.f), float2(1.f));
                end_uv = start_uv;
                stage = MouseStage::Dragging;
            } else if (action == Action::ACTION_RELEASED) {
                if (stage == MouseStage::Dragging && length(abs(start_uv - end_uv)) > 1e-2f)
                    stage = MouseStage::None;
                else {
                    start_uv = clamp(xy / make_float2(window_size), float2(0.f), float2(1.f));
                    stage = MouseStage::Clicking;
                }
            }
        } else if (button == MOUSE_BUTTON_2) {
            if (action == Action::ACTION_PRESSED) {
                camera_input.is_mouse_right_down = true;
            } else if (action == Action::ACTION_RELEASED) {
                camera_input.is_mouse_right_down = false;
            }
        }
    });
    utils.window()->set_cursor_position_callback([&](float2 xy) {
        if (stage == MouseStage::Dragging) {
            end_uv = clamp(xy / make_float2(window_size), float2(0.f), float2(1.f));
        }
        camera_input.mouse_cursor_pos = xy;
    });
    utils.window()->set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
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
                // light_move.create();
                // *light_move += float3(0, 0.1, 0);
            } break;
            case Key::KEY_S: {
                camera_input.is_back_dir_key_pressed = pressed;
                // light_move.create();
                // *light_move += float3(0, -0.1, 0);
            } break;
            case Key::KEY_A: {
                camera_input.is_left_dir_key_pressed = pressed;
                // light_move.create();
                // *light_move += float3(-0.1, 0, 0);
            } break;
            case Key::KEY_D: {
                camera_input.is_right_dir_key_pressed = pressed;
                // light_move.create();
                // *light_move += float3(0.1, 0, 0);
            } break;
            case Key::KEY_Q: {
                camera_input.is_up_dir_key_pressed = pressed;
                // light_move.create();
                // *light_move += float3(0, 0, -0.1);
            } break;
            case Key::KEY_E: {
                camera_input.is_down_dir_key_pressed = pressed;
                // light_move.create();
                // *light_move += float3(0, 0, 0.1);
            } break;
                // case Key::KEY_UP: {
                //     cube_move.create();
                //     *cube_move += float3(0, 0.1, 0);
                // } break;
                // case Key::KEY_DOWN: {
                //     cube_move.create();
                //     *cube_move += float3(0, -0.1, 0);
                // } break;
                // case Key::KEY_LEFT: {
                //     cube_move.create();
                //     *cube_move += float3(-0.1, 0, 0);
                // } break;
                // case Key::KEY_RIGHT: {
                //     cube_move.create();
                //     *cube_move += float3(0.1, 0, 0);
                // } break;
        }
    });
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    CameraController cam_controller;
    cam_controller.camera = &cam;

    utils.window()->set_window_size_callback([&](uint2 size) {
        window_size = size;
    });
    cam.fov = radians(80.f);
    while (!utils.should_close()) {
        if (reset) {
            reset = false;
            utils.reset_frame();
        }
        if (utils.window())
            utils.window()->poll_events();
        // reuse drag logic
        auto reset = world_scene->draw_gizmos(stage == MouseStage::Dragging, &utils, make_uint2(end_uv * make_float2(window_size)), window_size, cam.position, cam.far_plane, cam);
        auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
        if (any(window_size != utils.dst_image().size())) {
            utils.resize_swapchain(window_size);
            frame_index = 0;
        }
        if (reset) {
            frame_index = 0;
        }
        camera_input.viewport_size = make_float2(window_size);
        cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
        auto time = clk.toc();
        auto delta_time = (time - last_frame_time) * 1e-3f;
        cam_controller.grab_input_from_viewport(camera_input, delta_time);
        if (cam_controller.any_changed())
            frame_index = 0;
        last_frame_time = time;

        // click
        if (stage == MouseStage::Clicking) {
            click_mng.add_require("click", ClickRequire{.screen_uv = start_uv});
        }
        // set drag
        else if (stage == MouseStage::Dragging) {
            click_mng.add_frame_selection("dragging", min(start_uv, end_uv) * 2.f - 1.f, max(start_uv, end_uv) * 2.f - 1.f, true);
        }
        if (stage == MouseStage::Clicking || stage == MouseStage::Dragging) {
            dragged_object_ids.clear();
        }
        if (stage == MouseStage::Clicked) {
            auto click_result = click_mng.query_result("click");
            if (click_result) {
                dragged_object_ids.push_back(click_result->inst_id);
            }
        } else if (stage == MouseStage::Dragging) {
            auto dragging_result = click_mng.query_frame_selection("dragging");
            if (!dragging_result.empty())
                dragged_object_ids = std::move(dragging_result);
        }

        auto tick_stage = GraphicsUtils::TickStage::PathTracingPreview;
        constexpr uint sample = 256;
        // if (frame_index > sample) {
        //     tick_stage = GraphicsUtils::TickStage::PresentOfflineResult;
        // }
        click_mng.set_contour_objects(luisa::vector<uint>{dragged_object_ids});

        utils.tick(
            static_cast<float>(delta_time),
            frame_index,
            window_size,
            tick_stage);
        // if (frame_index == sample) {
        //     LUISA_INFO("Denoising..");
        //     utils.denoise();
        // }
        ++frame_index;
        switch (stage) {
            case MouseStage::Clicking:
                stage = MouseStage::Clicked;
                break;
        }
    }
    // rpc_hook.shutdown_remote();

    utils.dispose([&]() {
        world_scene.destroy();
        world::destroy_world();
        auto pipe_settings_json = utils.render_settings().serialize_to_json();
        if (pipe_settings_json.data()) {
            LUISA_INFO("{}", luisa::string_view{
                                 (char const *)pipe_settings_json.data(),
                                 pipe_settings_json.size()});
        }
        // destroy render-pipeline
    });
}
#endif
