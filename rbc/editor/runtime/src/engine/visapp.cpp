#include "RBCEditorRuntime/engine/VisApp.h"
#include "luisa/core/logging.h"
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
        (void)handled; // 暂时未使用，避免警告
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
    camera_input.viewport_size = {(float)(resolution.x),
                                  (float)(resolution.y)};
    auto time = clk.toc();

    auto delta_time = time - last_frame_time;
    last_frame_time = time;

    // handle camera control
    // 只有在非交互模式或拖动模式下才允许相机控制
    // 注意：在点击选择或框选模式下，禁用相机控制以避免冲突
    auto interaction_mode = interaction_manager.get_interaction_mode();
    bool allow_camera_control = (interaction_mode == ViewportInteractionManager::InteractionMode::None ||
                                 interaction_mode == ViewportInteractionManager::InteractionMode::Dragging);
    
    if (allow_camera_control) {
        cam_controller.grab_input_from_viewport(camera_input, static_cast<float>(delta_time));
        if (cam_controller.any_changed())
            frame_index = 0;
    }

    // 处理交互逻辑：根据交互状态设置点击管理器
    
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
