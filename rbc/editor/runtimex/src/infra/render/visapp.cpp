#include "RBCEditorRuntime/infra/render/visapp.h"

#include <rbc_graphics/make_device_config.h>
#include <rbc_render/click_manager.h>

#include <luisa/backends/ext/native_resource_ext.hpp>

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

    // 先处理交互管理器需要的按键（Ctrl等）
    interaction_manager.handle_key(key, action);

    // 然后处理相机控制相关的按键
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
        // 让交互管理器处理左键事件
        // 如果返回true，说明事件已被处理（选择/框选）
        // 如果返回false，说明是拖动已选物体模式（实际拖动逻辑需要后续实现）
        bool handled = interaction_manager.handle_mouse(button, action, xy, resolution);
        // 注意：拖动已选物体的实际逻辑（移动物体位置）需要后续实现
        // 当前只是标记为拖动模式，不进行实际拖动操作
        (void)handled;// 暂时未使用，避免警告
    } else if (button == MOUSE_BUTTON_RIGHT) {
        // 右键始终用于相机控制（旋转/平移相机）
        if (action == Action::ACTION_PRESSED) {
            camera_input.is_mouse_right_down = true;
        } else if (action == Action::ACTION_RELEASED) {
            camera_input.is_mouse_right_down = false;
        }
    }
}

void VisApp::handle_cursor_position(luisa::float2 xy) {
    // 更新交互管理器的鼠标位置
    interaction_manager.handle_cursor_position(xy, resolution);

    // 更新相机输入的鼠标位置
    camera_input.mouse_cursor_pos = xy;
}

void VisApp::update_camera(float delta_time) {
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
    camera_input.viewport_size = {(float)(resolution.x), (float)(resolution.y)};

    // 只有在非交互模式或右键拖动时才允许相机控制
    // 注意：在点击选择、框选模式或左键拖动已选物体时，禁用相机控制以避免冲突
    // 左键拖动已选物体时，应该让ViewportWidget处理拖放到NodeEditor
    auto interaction_mode = interaction_manager.get_interaction_mode();
    bool allow_camera_control = (interaction_mode == ViewportInteractionManager::InteractionMode::None);

    // 只有在非交互模式时才允许相机控制
    // Dragging模式（左键拖动已选物体）应该禁用相机控制，以便ViewportWidget可以处理拖放
    if (allow_camera_control) {
        cam_controller.grab_input_from_viewport(camera_input, delta_time);
        if (cam_controller.any_changed())
            frame_index = 0;
    }
}

void VisApp::update() {
    auto &click_mng = utils.render_settings().read_mut<ClickManager>();

    handle_reset();
    prepare_dx_states();

    dst_image_reseted = false;

    auto time = clk.toc();
    auto delta_time = time - last_frame_time;
    last_frame_time = time;

    // 更新相机（考虑交互模式）
    update_camera(static_cast<float>(delta_time));

    // 处理交互逻辑：根据交互状态设置点击管理器
    auto interaction_mode = interaction_manager.get_interaction_mode();

    if (interaction_mode == ViewportInteractionManager::InteractionMode::ClickSelect) {
        // 点击选择：在Pressed或WaitingResult状态时添加点击请求
        if (interaction_manager.is_click_selecting()) {
            auto selection_region = interaction_manager.get_selection_region();
            click_mng.add_require("click", ClickRequire{.screen_uv = selection_region.first});
        }
    } else if (interaction_mode == ViewportInteractionManager::InteractionMode::DragSelect) {
        if (interaction_manager.is_drag_selecting()) {
            // 框选：添加框选请求（在Dragging状态时添加）
            auto selection_region = interaction_manager.get_selection_region();
            // 转换为NDC坐标（-1到1）
            float2 min_ndc = selection_region.first * 2.f - 1.f;
            float2 max_ndc = selection_region.second * 2.f - 1.f;
            click_mng.add_frame_selection("dragging", min_ndc, max_ndc, true);
        }
    }

    // 更新交互管理器状态（查询选择结果）
    interaction_manager.update(click_mng);

    // 同步选择的对象ID列表
    dragged_object_ids = interaction_manager.get_selected_object_ids();

    // 设置轮廓对象（高亮显示选中的物体）
    click_mng.set_contour_objects(luisa::vector<uint>{dragged_object_ids});

    utils.tick(
        (float)delta_time,
        frame_index,
        resolution,
        GraphicsUtils::TickStage::RasterPreview);
}

VisApp::~VisApp() {
    dispose();
}

}// namespace rbc
