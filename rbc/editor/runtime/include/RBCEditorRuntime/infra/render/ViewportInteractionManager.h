#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/vstl/common.h>
#include <utility>

namespace rbc {

class ViewportInteractionManager {
public:
    enum class InteractionMode {
        None,// No Interaction
        ClickSelect,
        DragSelect,
        Dragging
    };

    enum class MouseState {
        Idle,
        Pressed,
        Dragging,
        Released,
        WaitingResult
    };

    struct InteractionState {
        InteractionMode mode = InteractionMode::None;
        MouseState mouse_state = MouseState::Idle;

        // Mouse Position (normalized uv)
        luisa::float2 start_uv = {0.f, 0.f};
        luisa::float2 end_uv = {0.f, 0.f};

        luisa::vector<uint> selected_object_ids;
        bool is_ctrl_down = false;
        static constexpr float drag_threshold = 1e-2f;
    };

public:
    ViewportInteractionManager() = default;
    ~ViewportInteractionManager() = default;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action);
    bool handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy, luisa::uint2 resolution);
    void handle_cursor_position(luisa::float2 xy, luisa::uint2 resolution);
    void update(struct ClickManager &click_manager);
    [[nodiscard]] const luisa::vector<uint> &get_selected_object_ids() const { return _state.selected_object_ids; }
    [[nodiscard]] InteractionMode get_interaction_mode() const { return _state.mode; }
    [[nodiscard]] std::pair<luisa::float2, luisa::float2> get_selection_region() const {
        return {luisa::min(_state.start_uv, _state.end_uv), luisa::max(_state.start_uv, _state.end_uv)};
    }
    [[nodiscard]] bool is_drag_selecting() const {
        return _state.mode == InteractionMode::DragSelect && _state.mouse_state == MouseState::Dragging;
    }
    [[nodiscard]] bool is_click_selecting() const {
        return _state.mode == InteractionMode::ClickSelect &&
               (_state.mouse_state == MouseState::Pressed || _state.mouse_state == MouseState::WaitingResult);
    }
    void reset();
private:
    InteractionMode determine_interaction_mode(bool has_selection, bool is_ctrl_down) const;
    bool is_repeat_selection(const luisa::vector<uint> &new_selection) const;
    void update_selection(const luisa::vector<uint> &new_selection, bool is_repeat);
    InteractionState _state;
};

}// namespace rbc