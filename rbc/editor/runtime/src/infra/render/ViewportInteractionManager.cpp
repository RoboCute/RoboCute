#include "RBCEditorRuntime/infra/render/ViewportInteractionManager.h"
#include <rbc_render/click_manager.h>
#include <luisa/core/logging.h>
#include <algorithm>

using namespace luisa;
using namespace luisa::compute;

namespace rbc {

void ViewportInteractionManager::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    bool pressed = (action == Action::ACTION_PRESSED);

    switch (key) {
        case Key::KEY_LEFT_CONTROL:
        case Key::KEY_RIGHT_CONTROL: {
            state_.is_ctrl_down = pressed;
            // 如果Ctrl键释放，且当前处于框选模式，切换到普通选择模式
            if (!pressed && state_.mode == InteractionMode::DragSelect && state_.mouse_state == MouseState::Idle) {
                state_.mode = InteractionMode::None;
            }
        } break;
        default:
            break;
    }
}

bool ViewportInteractionManager::handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy, luisa::uint2 resolution) {
    if (button != MOUSE_BUTTON_LEFT) {
        return false;// 非左键事件，不处理
    }

    float2 uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));

    if (action == Action::ACTION_PRESSED) {
        // 鼠标按下
        state_.start_uv = uv;
        state_.end_uv = uv;
        state_.mouse_state = MouseState::Pressed;

        // 确定交互模式
        // 注意：即使有选择，也应该先进入点击选择模式，以便检测点击空白处
        // 如果后续拖动距离超过阈值，再切换到拖动模式
        bool has_selection = !state_.selected_object_ids.empty();
        if (state_.is_ctrl_down) {
            // Ctrl+左键 = 框选模式
            state_.mode = InteractionMode::DragSelect;
        } else {
            // 普通左键：先进入点击选择模式，以便检测点击空白处
            // 如果后续拖动，会在handle_cursor_position中切换到拖动模式
            state_.mode = InteractionMode::ClickSelect;
        }

        return true;// 事件已处理
    } else if (action == Action::ACTION_RELEASED) {
        // 鼠标释放
        if (state_.mouse_state == MouseState::Dragging) {
            // 拖动结束
            float drag_distance = length(abs(state_.start_uv - state_.end_uv));
            if (drag_distance > InteractionState::drag_threshold) {
                // 确实是拖动
                if (state_.mode == InteractionMode::DragSelect) {
                    // 框选完成，等待update中查询结果
                    state_.mouse_state = MouseState::Released;
                } else if (state_.mode == InteractionMode::Dragging) {
                    // 拖动已选物体模式：保持选择状态，结束拖动
                    state_.mouse_state = MouseState::Idle;
                    state_.mode = InteractionMode::None;
                } else {
                    // 其他拖动模式
                    state_.mouse_state = MouseState::Idle;
                    state_.mode = InteractionMode::None;
                }
            } else {
                // 实际上是点击（拖动距离很小）
                // 切换到点击选择模式，以便检测点击空白处
                state_.mouse_state = MouseState::WaitingResult;
                state_.mode = InteractionMode::ClickSelect;
            }
        } else if (state_.mouse_state == MouseState::Pressed) {
            // 快速点击（没有拖动）
            // 如果是在点击选择模式，需要等待查询结果
            if (state_.mode == InteractionMode::ClickSelect) {
                state_.mouse_state = MouseState::WaitingResult;
            } else {
                state_.mouse_state = MouseState::Released;
            }
        }

        return true;// 事件已处理
    }

    return false;
}

void ViewportInteractionManager::handle_cursor_position(luisa::float2 xy, luisa::uint2 resolution) {
    if (state_.mouse_state == MouseState::Pressed || state_.mouse_state == MouseState::Dragging) {
        float2 uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));
        state_.end_uv = uv;

        // 检查是否应该从按下状态切换到拖动状态
        if (state_.mouse_state == MouseState::Pressed) {
            float drag_distance = length(abs(state_.start_uv - state_.end_uv));
            if (drag_distance > InteractionState::drag_threshold) {
                state_.mouse_state = MouseState::Dragging;

                // 重新确定交互模式（因为拖动距离可能改变模式）
                bool has_selection = !state_.selected_object_ids.empty();
                if (state_.is_ctrl_down) {
                    // Ctrl+拖动 = 框选模式（无论是否有选择）
                    state_.mode = InteractionMode::DragSelect;
                } else if (has_selection) {
                    // 有选择且拖动 = 拖动已选物体模式
                    state_.mode = InteractionMode::Dragging;
                } else {
                    // 无选择且拖动 = 保持点击选择模式（不进入框选，框选需要Ctrl）
                    // 注意：这种情况下，拖动会被视为无效操作，不会进行框选
                    state_.mode = InteractionMode::ClickSelect;
                }
            }
        }
    }
}

void ViewportInteractionManager::update(class ClickManager &click_manager) {
    // 处理点击选择
    if (state_.mode == InteractionMode::ClickSelect) {
        if (state_.mouse_state == MouseState::WaitingResult || state_.mouse_state == MouseState::Released) {
            // 查询点击结果
            auto click_result = click_manager.query_result("click");
            if (click_result && click_result->inst_id != ~0u) {
                // 选中了物体
                luisa::vector<uint> new_selection{click_result->inst_id};
                bool is_repeat = is_repeat_selection(new_selection);
                update_selection(new_selection, is_repeat);
            } else {
                // 点击空白处或没有选中物体，清空选择
                state_.selected_object_ids.clear();
            }
            state_.mouse_state = MouseState::Idle;
            state_.mode = InteractionMode::None;
        }
    }
    // 处理框选
    else if (state_.mode == InteractionMode::DragSelect) {
        if (state_.mouse_state == MouseState::Dragging) {
            // 框选进行中，在update循环外部不需要查询结果
            // 结果会在下一帧查询
        } else if (state_.mouse_state == MouseState::Released) {
            // 框选完成，查询结果
            auto dragging_result = click_manager.query_frame_selection("dragging");
            if (!dragging_result.empty()) {
                bool is_repeat = is_repeat_selection(dragging_result);
                update_selection(dragging_result, is_repeat);
            } else {
                // 框选没有选中任何物体，清空选择（除非是Ctrl+框选）
                if (!state_.is_ctrl_down) {
                    state_.selected_object_ids.clear();
                }
            }
            state_.mouse_state = MouseState::Idle;
            state_.mode = InteractionMode::None;
        }
    }
    // 处理拖动模式：在拖动模式下，允许相机控制，但保持选择状态
    else if (state_.mode == InteractionMode::Dragging) {
        // 拖动模式下，保持选择状态不变，让相机控制处理拖动
        // 这里不需要做任何操作，选择状态会保持
    }
}

ViewportInteractionManager::InteractionMode ViewportInteractionManager::determine_interaction_mode(bool has_selection, bool is_ctrl_down) const {
    // 优先级：拖动已选物体 > 框选 > 点击选择

    // 如果有选择且不是Ctrl+点击，优先考虑拖动已选物体
    // 注意：拖动已选物体的实际逻辑需要后续实现（移动物体位置）
    // 当前返回Dragging模式，表示应该拖动物体而不是框选
    if (has_selection && !is_ctrl_down) {
        return InteractionMode::Dragging;
    }

    // Ctrl+左键 = 框选模式（无论是否有选择）
    if (is_ctrl_down) {
        return InteractionMode::DragSelect;
    }

    // 普通左键 = 点击选择
    return InteractionMode::ClickSelect;
}

bool ViewportInteractionManager::is_repeat_selection(const luisa::vector<uint> &new_selection) const {
    if (state_.selected_object_ids.empty() || new_selection.empty()) {
        return false;
    }

    // 检查新选择是否完全包含在当前选择中
    // 如果新选择的第一个对象在当前选择中，认为是重复选择
    for (uint new_id : new_selection) {
        bool found = false;
        for (uint existing_id : state_.selected_object_ids) {
            if (new_id == existing_id) {
                found = true;
                break;
            }
        }
        if (found) {
            return true;// 至少有一个重复
        }
    }

    return false;
}

void ViewportInteractionManager::update_selection(const luisa::vector<uint> &new_selection, bool is_repeat) {
    if (is_repeat) {
        // 重复选择：保持当前选择不变
        // 不做任何操作
        return;
    }

    // 非重复选择：更新选择状态
    if (state_.is_ctrl_down && !state_.selected_object_ids.empty()) {
        // Ctrl+选择：添加到当前选择（去重）
        for (uint new_id : new_selection) {
            bool exists = false;
            for (uint existing_id : state_.selected_object_ids) {
                if (new_id == existing_id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                state_.selected_object_ids.push_back(new_id);
            }
        }
    } else {
        // 普通选择：替换当前选择
        state_.selected_object_ids = new_selection;
    }
}

void ViewportInteractionManager::reset() {
    state_ = InteractionState{};
}

}// namespace rbc
