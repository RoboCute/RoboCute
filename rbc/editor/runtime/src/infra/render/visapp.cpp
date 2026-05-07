#include "RBCEditorRuntime/infra/render/visapp.h"

#include <rbc_graphics/make_device_config.h>
#include <rbc_render/click_manager.h>

#include <luisa/backends/ext/native_resource_ext.hpp>
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void VisApp::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    bool pressed = false;
    if (action == Action::ACTION_PRESSED) {
        pressed = true;
    } else if (action == Action::ACTION_RELEASED) {
        pressed = false;
    } else {
        return;
    }
    interaction_manager.handle_key(key, action);
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
        default:
            break;
    }
}

void VisApp::handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) {
    if (button == MOUSE_BUTTON_LEFT) {
        interaction_manager.handle_mouse(button, action, xy, resolution);
    } else if (button == MOUSE_BUTTON_RIGHT) {
        if (action == Action::ACTION_PRESSED) {
            camera_input.is_mouse_right_down = true;
        } else if (action == Action::ACTION_RELEASED) {
            camera_input.is_mouse_right_down = false;
        }
    }
}

void VisApp::handle_cursor_position(luisa::float2 xy) {
    interaction_manager.handle_cursor_position(xy, resolution);
    camera_input.mouse_cursor_pos = xy;
}

void VisApp::update_camera(float delta_time) {
    auto &cam = utils.render_settings(pipe_ctx).read_mut<Camera>();
    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    camera_input.viewport_size = {(float)(resolution.x), (float)(resolution.y)};
    auto interaction_mode = interaction_manager.get_interaction_mode();
    bool allow_camera_control = (interaction_mode == ViewportInteractionManager::InteractionMode::None);
    if (allow_camera_control) {
        cam_controller.grab_input_from_viewport(camera_input, delta_time);
        if (cam_controller.any_changed())
            frame_index = 0;
    }
}

void VisApp::update() {
    auto &click_mng = utils.render_settings(pipe_ctx).read_mut<ClickManager>();

    handle_reset();
    prepare_dx_states();

    dst_image_reset = false;

    auto time = clk.toc();
    auto delta_time = time - last_frame_time;
    last_frame_time = time;

    // Update camera (taking interaction mode into account)
    update_camera(static_cast<float>(delta_time));

    // Process interaction logic: set click manager according to interaction state
    auto interaction_mode = interaction_manager.get_interaction_mode();

    if (interaction_mode == ViewportInteractionManager::InteractionMode::ClickSelect) {
        // Click select: add click request when in Pressed or WaitingResult state
        if (interaction_manager.is_click_selecting()) {
            auto selection_region = interaction_manager.get_selection_region();
            click_mng.add_require("click", ClickRequire{.screen_uv = selection_region.first});
        }
    } else if (interaction_mode == ViewportInteractionManager::InteractionMode::DragSelect) {
        if (interaction_manager.is_drag_selecting()) {
            // Box select: add box-select request (added when in Dragging state)
            auto selection_region = interaction_manager.get_selection_region();
            // Convert to NDC coordinates (-1 to 1)
            float2 min_ndc = selection_region.first * 2.f - 1.f;
            float2 max_ndc = selection_region.second * 2.f - 1.f;
            click_mng.add_frame_selection("dragging", min_ndc, max_ndc, true);
        }
    }

    // Update interaction manager state (query selection results)
    interaction_manager.update(click_mng);

    // Sync selected object ID list
    dragged_object_ids = interaction_manager.get_selected_object_ids();

    // Set contour objects (highlight selected objects)
    click_mng.set_contour_objects(luisa::vector<uint>{dragged_object_ids});

    utils.tick(
        GraphicsUtils::TickStage::RasterPreview);
}

VisApp::~VisApp() {
    dispose();
}

}// namespace rbc
