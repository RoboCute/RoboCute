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
            _state.is_ctrl_down = pressed;
            // If Ctrl is released and currently in drag-select mode, switch to normal selection mode
            if (!pressed && _state.mode == InteractionMode::DragSelect && _state.mouse_state == MouseState::Idle) {
                _state.mode = InteractionMode::None;
            }
        } break;
        default:
            break;
    }
}

bool ViewportInteractionManager::handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy, luisa::uint2 resolution) {
    if (button != MOUSE_BUTTON_LEFT) {
        return false;// Non-left-button event, ignore
    }

    float2 uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));

    if (action == Action::ACTION_PRESSED) {
        // Mouse pressed
        _state.start_uv = uv;
        _state.end_uv = uv;
        _state.mouse_state = MouseState::Pressed;

        // Determine interaction mode
        // Note: even if there is a selection, enter click-select mode first to detect clicking on empty space
        // If subsequent drag distance exceeds threshold, switch to drag mode
        if (_state.is_ctrl_down) {
            // Ctrl+Left = drag-select mode
            _state.mode = InteractionMode::DragSelect;
        } else {
            // Normal left click: enter click-select mode first to detect clicking on empty space
            // If subsequent drag occurs, will switch to drag mode in handle_cursor_position
            _state.mode = InteractionMode::ClickSelect;
        }

        return true;// Event handled
    } else if (action == Action::ACTION_RELEASED) {
        // Mouse released
        if (_state.mouse_state == MouseState::Dragging) {
            // Drag ended
            float drag_distance = length(abs(_state.start_uv - _state.end_uv));
            if (drag_distance > InteractionState::drag_threshold) {
                // Confirmed drag
                if (_state.mode == InteractionMode::DragSelect) {
                    // Drag-select complete, wait for query result in update
                    _state.mouse_state = MouseState::Released;
                } else if (_state.mode == InteractionMode::Dragging) {
                    // Drag selected objects mode: keep selection state, end dragging
                    _state.mouse_state = MouseState::Idle;
                    _state.mode = InteractionMode::None;
                } else {
                    // Other drag modes
                    _state.mouse_state = MouseState::Idle;
                    _state.mode = InteractionMode::None;
                }
            } else {
                // Actually a click (drag distance very small)
                // Switch to click-select mode to detect clicking on empty space
                _state.mouse_state = MouseState::WaitingResult;
                _state.mode = InteractionMode::ClickSelect;
            }
        } else if (_state.mouse_state == MouseState::Pressed) {
            // Quick click (no drag)
            // If in click-select mode, need to wait for query result
            if (_state.mode == InteractionMode::ClickSelect) {
                _state.mouse_state = MouseState::WaitingResult;
            } else {
                _state.mouse_state = MouseState::Released;
            }
        }

        return true;// Event handled
    }

    return false;
}

void ViewportInteractionManager::handle_cursor_position(luisa::float2 xy, luisa::uint2 resolution) {
    if (_state.mouse_state == MouseState::Pressed || _state.mouse_state == MouseState::Dragging) {
        float2 uv = clamp(xy / make_float2(resolution), float2(0.f), float2(1.f));
        _state.end_uv = uv;

        // Check whether to switch from pressed to dragging state
        if (_state.mouse_state == MouseState::Pressed) {
            float drag_distance = length(abs(_state.start_uv - _state.end_uv));
            if (drag_distance > InteractionState::drag_threshold) {
                _state.mouse_state = MouseState::Dragging;

                // Re-determine interaction mode (drag distance may change mode)
                bool has_selection = !_state.selected_object_ids.empty();
                if (_state.is_ctrl_down) {
                    // Ctrl+Drag = drag-select mode (regardless of selection)
                    _state.mode = InteractionMode::DragSelect;
                } else if (has_selection) {
                    // Has selection and dragging = drag selected objects mode
                    _state.mode = InteractionMode::Dragging;
                } else {
                    // No selection and dragging = keep click-select mode (no drag-select, drag-select needs Ctrl)
                    // Note: in this case, dragging is treated as invalid operation, no drag-select
                    _state.mode = InteractionMode::ClickSelect;
                }
            }
        }
    }
}

void ViewportInteractionManager::update(class ClickManager &click_manager) {
    // Process click select
    if (_state.mode == InteractionMode::ClickSelect) {
        if (_state.mouse_state == MouseState::WaitingResult || _state.mouse_state == MouseState::Released) {
            // Query click result
            auto click_result = click_manager.query_result("click");
            if (click_result && click_result->inst_id != ~0u) {
                // Object selected
                luisa::vector<uint> new_selection{click_result->inst_id};
                bool is_repeat = is_repeat_selection(new_selection);
                update_selection(new_selection, is_repeat);
            } else {
                // Clicked on empty space or no object selected, clear selection
                _state.selected_object_ids.clear();
            }
            _state.mouse_state = MouseState::Idle;
            _state.mode = InteractionMode::None;
        }
    }
    // Process drag select
    else if (_state.mode == InteractionMode::DragSelect) {
        if (_state.mouse_state == MouseState::Dragging) {
            // Drag-select in progress, no need to query result outside update loop
            // Result will be queried in next frame
        } else if (_state.mouse_state == MouseState::Released) {
            // Drag-select complete, query result
            auto dragging_result = click_manager.query_frame_selection("dragging");
            if (!dragging_result.empty()) {
                bool is_repeat = is_repeat_selection(dragging_result);
                update_selection(dragging_result, is_repeat);
            } else {
                // Drag-select selected no objects, clear selection (unless Ctrl+drag-select)
                if (!_state.is_ctrl_down) {
                    _state.selected_object_ids.clear();
                }
            }
            _state.mouse_state = MouseState::Idle;
            _state.mode = InteractionMode::None;
        }
    }
    // Process dragging mode: in dragging mode, allow camera control but keep selection state
    else if (_state.mode == InteractionMode::Dragging) {
        // In dragging mode, keep selection unchanged, let camera control handle dragging
        // No action needed here, selection state will be kept
    }
}

ViewportInteractionManager::InteractionMode ViewportInteractionManager::determine_interaction_mode(bool has_selection, bool is_ctrl_down) const {
    // Priority: drag selected objects > drag-select > click-select

    // If has selection and not Ctrl+click, prioritize dragging selected objects
    // Note: actual logic for dragging selected objects needs future implementation (moving object positions)
    // Currently returns Dragging mode, meaning should drag objects instead of drag-select
    if (has_selection && !is_ctrl_down) {
        return InteractionMode::Dragging;
    }

    // Ctrl+Left = drag-select mode (regardless of selection)
    if (is_ctrl_down) {
        return InteractionMode::DragSelect;
    }

    // Normal left click = click-select
    return InteractionMode::ClickSelect;
}

bool ViewportInteractionManager::is_repeat_selection(const luisa::vector<uint> &new_selection) const {
    if (_state.selected_object_ids.empty() || new_selection.empty()) {
        return false;
    }

    // Check if new selection is fully contained in current selection
    // If the first object of new selection is in current selection, consider it a repeat selection
    for (uint new_id : new_selection) {
        bool found = false;
        for (uint existing_id : _state.selected_object_ids) {
            if (new_id == existing_id) {
                found = true;
                break;
            }
        }
        if (found) {
            return true;// At least one duplicate
        }
    }

    return false;
}

void ViewportInteractionManager::update_selection(const luisa::vector<uint> &new_selection, bool is_repeat) {
    if (is_repeat) {
        // Repeat selection: keep current selection unchanged
        // Do nothing
        return;
    }

    // Non-repeat selection: update selection state
    if (_state.is_ctrl_down && !_state.selected_object_ids.empty()) {
        // Ctrl+Select: add to current selection (deduplicate)
        for (uint new_id : new_selection) {
            bool exists = false;
            for (uint existing_id : _state.selected_object_ids) {
                if (new_id == existing_id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                _state.selected_object_ids.push_back(new_id);
            }
        }
    } else {
        // Normal selection: replace current selection
        _state.selected_object_ids = new_selection;
    }
}

void ViewportInteractionManager::reset() {
    _state = InteractionState{};
}

}// namespace rbc
