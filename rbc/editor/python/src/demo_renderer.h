#pragma once

#include "RBCEditorRuntime/infra/render/app_base.h"
#include <luisa/core/stl/string.h>
#include <luisa/gui/input.h>

namespace rbc::pyside_demo {

/**
 * @brief Minimal renderer implementing the editor's IRenderer interface.
 *
 * This is intentionally a stub used to validate single-process PySide6
 * embedding of the LC viewport widget.  It records input state and a
 * configurable clear colour, but does not drive the full LC render graph.
 * Swapping this for rbc::VisApp (or any RenderAppBase subclass) is the
 * next step once device/context initialisation is wired.
 */
class DemoRenderer : public IRenderer {
public:
    void init(const char *program_path, const char *backend_name) override;
    void process_qt_handle(QRhiNativeHandles &qt_rhi_handle) override;
    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button,
                      luisa::compute::Action action,
                      luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;
    [[nodiscard]] uint64_t get_present_texture(uint width, uint height) override;

    // Demo-only controls exposed to Python.
    void set_clear_color(float r, float g, float b);
    void set_camera_distance(float distance);
    void reset_camera();

    [[nodiscard]] float camera_distance() const { return _camera_distance; }
    [[nodiscard]] const std::array<float, 3> &clear_color() const { return _clear_color; }
    [[nodiscard]] luisa::compute::Key last_key() const { return _last_key; }
    [[nodiscard]] bool initialized() const { return _initialized; }

private:
    bool _initialized = false;
    luisa::string _program_path;
    luisa::string _backend_name;

    std::array<float, 3> _clear_color = {0.2f, 0.25f, 0.3f};
    float _camera_distance = 5.0f;

    luisa::compute::Key _last_key = luisa::compute::KEY_UNKNOWN;
    luisa::compute::MouseButton _last_button = luisa::compute::MOUSE_BUTTON_UNKNOWN;
    luisa::float2 _last_mouse_pos = {};
};

}// namespace rbc::pyside_demo
