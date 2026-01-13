#include <rbc_app/camera_controller.h>
#include <luisa/core/logging.h>
namespace rbc {
// phase
void CameraController::grab_input_from_viewport(Input const &input, float delta_time) {
    LUISA_ASSERT(camera != nullptr);

    // get last widget size
    {
        _viewport_size = input.viewport_size;
    }

    // update mouse info
    {
        auto now_mouse_pos = input.mouse_cursor_pos;
        _mouse_delta = now_mouse_pos - _mouse_pos;
        _mouse_pos = now_mouse_pos;
    }

    // get mouse state

    // trigger rotation
    if (!_is_rotating && (input.is_mouse_right_down)) {
        _is_rotating = true;
    }

    // end rotation
    if (_is_rotating && !(input.is_mouse_right_down)) {
        _is_rotating = false;
    }

    // trigger panning
    if (!_is_panning && (input.is_mouse_middle_down)) {
        _is_panning = true;
    }

    // end panning
    if (_is_panning && !(input.is_mouse_middle_down)) {
        _is_panning = false;
    }
    // TODO: may need set cursor
    // ImGui::SetMouseCursor((_is_panning || _is_rotating) ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Arrow);

    // get mouse wheel
    _mouse_wheel = input.is_hovered ? input.mouse_wheel : 0;

    // get key state
    bool allow_move = _is_rotating;
    _shift_down = input.is_shift_down;
    _space_down = input.is_space_down;
    _move_forward = allow_move && input.is_front_dir_key_pressed;
    _move_backward = allow_move && input.is_back_dir_key_pressed;
    _move_left = allow_move && input.is_left_dir_key_pressed;
    _move_right = allow_move && input.is_right_dir_key_pressed;
    _move_up = allow_move && input.is_up_dir_key_pressed;
    _move_down = allow_move && input.is_down_dir_key_pressed;

    _update(delta_time);
}

// move state
bool CameraController::is_moving() const {
    return
        // key move
        _move_forward ||
        _move_backward ||
        _move_left ||
        _move_right ||
        _move_up ||
        _move_down ||
        // mouse input triggered
        (_is_panning && any(_mouse_delta != float2{0, 0})) ||
        // mouse wheel
        _mouse_wheel != 0.f ||
        false;
}
bool CameraController::is_rotating() const {
    return _is_rotating && any(_mouse_delta != float2{0, 0});
}
bool CameraController::any_changed() const {
    return is_moving() || is_rotating();
}

// update
void CameraController::_update(float delta_time) {
    if (!_controlling) return;
    LUISA_ASSERT(camera != nullptr);

    // camera dirs
    double3 forward = camera->dir_forward();
    double3 right = camera->dir_right();
    double3 up = camera->dir_up();

    // rotate & pan
    if (_is_rotating) {
        camera->rotation_yaw -= _mouse_delta.x / _viewport_size.x * rotation_speed;
        camera->rotation_pitch -= _mouse_delta.y / _viewport_size.y * rotation_speed;
        camera->rotation_pitch = clamp(camera->rotation_pitch, -pi * 0.48, pi * 0.48);
    } else if (_is_panning) {
        camera->position -= make_double3(_mouse_delta.x / _viewport_size.x * (double)move_speed * right);
        camera->position -= make_double3(_mouse_delta.y / _viewport_size.y * (double)move_speed * up);
    }

    // wsad control
    double move_speed_real = _shift_down ? move_speed_fast : (_space_down ? move_speed_slow : move_speed);
    double move_delta = move_speed_real * delta_time;
    if (_move_forward) {
        camera->position += make_double3(forward * move_delta);
    }
    if (_move_backward) {
        camera->position -= make_double3(forward * move_delta);
    }
    if (_move_left) {
        camera->position -= make_double3(right * move_delta);
    }
    if (_move_right) {
        camera->position += make_double3(right * move_delta);
    }
    if (_move_up) {
        camera->position.y += move_delta;
    }
    if (_move_down) {
        camera->position.y -= move_delta;
    }

    // zoom
    if (_mouse_wheel) {
        camera->position += make_double3(forward * (double)_mouse_wheel * move_speed_real * (double)wheel_move_scale / 100.0);
    }
}
}// namespace rbc