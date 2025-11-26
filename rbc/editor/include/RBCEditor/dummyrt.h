#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/rhi/resource.h>
#include <luisa/backends/ext/native_resource_ext_interface.h>

struct Onb {
    luisa::compute::float3 tangent;
    luisa::compute::float3 binormal;
    luisa::compute::float3 normal;
};

struct Camera {
    luisa::compute::float3 position;
    luisa::compute::float3 front;
    luisa::compute::float3 up;
    luisa::compute::float3 right;
    float fov;
};

class FPVCameraController {

private:
    Camera &_camera;
    float _move_speed;
    float _rotate_speed;
    float _zoom_speed;

public:
    explicit FPVCameraController(Camera &camera,
                                 float move_speed,
                                 float rotate_speed,
                                 float zoom_speed) noexcept
        : _camera{camera},
          _move_speed{move_speed},
          _rotate_speed{rotate_speed},
          _zoom_speed{zoom_speed} {
        // make sure the camera is valid
        _camera.front = normalize(_camera.front);
        _camera.right = normalize(cross(_camera.front, _camera.up));
        _camera.up = normalize(cross(_camera.right, _camera.front));
        _camera.fov = std::clamp(_camera.fov, 1.f, 179.f);
    }
    void zoom(float scale) noexcept { _camera.fov = std::clamp(_camera.fov * std::pow(2.f, -scale * _zoom_speed), 1.f, 179.f); }
    void move_right(float dx) noexcept { _camera.position += _camera.right * dx * _move_speed; }
    void move_up(float dy) noexcept { _camera.position += _camera.up * dy * _move_speed; }
    void move_forward(float dz) noexcept { _camera.position += _camera.front * dz * _move_speed; }
    void rotate_roll(float angle) noexcept {
        auto m = make_float3x3(rotation(_camera.front, luisa::radians(_rotate_speed * angle)));
        _camera.up = normalize(m * _camera.up);
        _camera.right = normalize(m * _camera.right);
    }
    void rotate_yaw(float angle) noexcept {
        auto m = make_float3x3(rotation(_camera.up, luisa::radians(_rotate_speed * angle)));
        _camera.front = normalize(m * _camera.front);
        _camera.right = normalize(m * _camera.right);
    }
    void rotate_pitch(float angle) noexcept {
        auto m = make_float3x3(rotation(_camera.right, luisa::radians(_rotate_speed * angle)));
        _camera.front = normalize(m * _camera.front);
        _camera.up = normalize(m * _camera.up);
    }
    [[nodiscard]] auto move_speed() const noexcept { return _move_speed; }
    [[nodiscard]] auto rotate_speed() const noexcept { return _rotate_speed; }
    [[nodiscard]] auto zoom_speed() const noexcept { return _zoom_speed; }
    void set_move_speed(float speed) noexcept { _move_speed = speed; }
    void set_rotate_speed(float speed) noexcept { _rotate_speed = speed; }
    void set_zoom_speed(float speed) noexcept { _zoom_speed = speed; }
};

struct App {
    luisa::compute::Device device;
    luisa::compute::Stream stream;
    luisa::compute::CommandList cmd_list;

    luisa::compute::BindlessArray heap;
    luisa::compute::Buffer<luisa::compute::float3> vertex_buffer;
    luisa::compute::Image<float> framebuffer;
    luisa::compute::Image<float> ldr_image;
    luisa::compute::Image<float> accum_image;
    luisa::compute::Image<uint> seed_image;
    luisa::compute::Image<float> dummy_image;

    luisa::compute::DeviceConfigExt *device_config_ext{};

    luisa::compute::Shader<2, luisa::compute::Image<float>> clear_shader;
    luisa::compute::Shader<2, luisa::compute::Image<float>, float, luisa::compute::float2> draw_shader;
    luisa::compute::Shader<2, luisa::compute::Image<float>, luisa::compute::Image<uint>, Camera, luisa::compute::Accel, luisa::uint2> raytracing_shader;
    luisa::compute::Shader<2, luisa::compute::Image<float>, luisa::compute::Image<float>, float, bool> hdr2ldr_shader;
    luisa::compute::Shader<2, luisa::compute::Image<float>, luisa::compute::Image<float>> accumulate_shader;
    luisa::compute::Shader<2, luisa::compute::Image<uint>> make_sampler_shader;

    luisa::uint2 resolution;
    luisa::Clock clk;
    double delta_time;
    double last_time;

    Camera camera;
    luisa::unique_ptr<FPVCameraController> camera_controller;
    luisa::compute::Accel accel;
    bool is_dirty;
    void *vk_physical_device{};
    void *vk_instance{};
    uint32_t vk_queue_family_idx{};
    luisa::uint2 dx_adaptor_luid;

    void init(luisa::compute::Context &ctx, const char *backend_name);
    uint64_t create_texture(uint width, uint height);
    void update();
    void handle_key(luisa::compute::Key key);
    void *init_vulkan(luisa::compute::Context &ctx);
    ~App();
};
