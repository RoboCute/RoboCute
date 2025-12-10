#pragma once
#include <rbc_graphics/matrix.h>
#include <luisa/vstl/lmdb.hpp>
#include <luisa/core/basic_types.h>
#include <rbc_graphics/camera.h>

namespace rbc {
using namespace luisa;

struct RBC_APP_API CameraController {
    struct Input {
        float2 mouse_cursor_pos;
        float2 viewport_size;
        float mouse_wheel{};
        bool is_hovered : 1 {};
        bool is_mouse_middle_down : 1 {};
        bool is_mouse_right_down : 1 {};
        bool is_front_dir_key_pressed : 1 {};
        bool is_back_dir_key_pressed : 1 {};
        bool is_left_dir_key_pressed : 1 {};
        bool is_right_dir_key_pressed : 1 {};
        bool is_up_dir_key_pressed : 1 {};
        bool is_down_dir_key_pressed : 1 {};
        bool is_shift_down : 1 {};
        bool is_space_down : 1 {};
    };
    // phase
    void grab_input_from_viewport(Input const &input, float delta_time);//! NOTE. just grad input, camera update won't trigger in this phase

    // move state
    [[nodiscard]] bool is_moving() const;
    [[nodiscard]] bool is_rotating() const;
    [[nodiscard]] bool any_changed() const;

private:
    // update
    void _update(float delta_time);// update camera state, and trigger some events

public:

    // camera state
    Camera *camera;

    // camera config
    float move_speed_fast = 2.0f;
    float move_speed = 1.0f;
    float move_speed_slow = 0.3f;
    float wheel_move_scale = 4.f;
    float rotation_speed = 4.0f;
    void set_controlling(bool value) {
        _controlling = value;
    }
    [[nodiscard]] auto controlling() const { return _controlling; }

private:
    // input state
    float2 _viewport_size = float2::zero();
    float2 _mouse_pos = float2::zero();
    float2 _mouse_delta = float2::zero();
    float _mouse_wheel = 0.0f;
    bool _is_rotating : 1 = false;
    bool _is_panning : 1 = false;
    bool _shift_down : 1 = false;
    bool _space_down : 1 = false;
    bool _move_forward : 1 = false;
    bool _move_backward : 1 = false;
    bool _move_left : 1 = false;
    bool _move_right : 1 = false;
    bool _move_up : 1 = false;
    bool _move_down : 1 = false;
    bool _controlling : 1 = true;
};
}// namespace rbc