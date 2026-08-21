#include "demo_renderer.h"

#include <luisa/core/logging.h>

namespace rbc::pyside_demo {

void DemoRenderer::init(const char *program_path, const char *backend_name) {
    if (_initialized) {
        return;
    }
    _program_path = program_path ? program_path : "";
    _backend_name = backend_name ? backend_name : "dx";
    LUISA_INFO(
        "[rbc_editor_py] DemoRenderer::init(program_path='{}', backend='{}')",
        _program_path, _backend_name);
    _initialized = true;
}

void DemoRenderer::process_qt_handle(QRhiNativeHandles & /*qt_rhi_handle*/) {
    // For a real LC renderer this would inject the LC D3D12 device/queue
    // into QRhi's native handles.  The stub creates its own RHI device.
}

void DemoRenderer::update() {
    // No real render graph update in the stub.
}

void DemoRenderer::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    _last_key = key;
    LUISA_INFO(
        "[rbc_editor_py] DemoRenderer::handle_key(key={}, action={})",
        static_cast<int>(key), static_cast<int>(action));
}

void DemoRenderer::handle_mouse(luisa::compute::MouseButton button,
                                luisa::compute::Action action,
                                luisa::float2 xy) {
    _last_button = button;
    _last_mouse_pos = xy;
    LUISA_INFO(
        "[rbc_editor_py] DemoRenderer::handle_mouse(button={}, action={}, xy=({}, {}))",
        static_cast<int>(button), static_cast<int>(action), xy.x, xy.y);
}

void DemoRenderer::handle_cursor_position(luisa::float2 xy) {
    _last_mouse_pos = xy;
}

uint64_t DemoRenderer::get_present_texture(uint width, uint height) {
    // Returning 0 tells RhiWindow that no LC-presentable texture is available.
    // A temporary guard in RhiWindow::render skips the fullscreen pass when
    // the handle is 0, so the widget stays black instead of crashing.
    LUISA_INFO(
        "[rbc_editor_py] DemoRenderer::get_present_texture({}, {})",
        width, height);
    return 0;
}

void DemoRenderer::set_clear_color(float r, float g, float b) {
    _clear_color = {r, g, b};
    LUISA_INFO(
        "[rbc_editor_py] set_clear_color({}, {}, {})",
        r, g, b);
}

void DemoRenderer::set_camera_distance(float distance) {
    _camera_distance = distance;
    LUISA_INFO(
        "[rbc_editor_py] set_camera_distance({})",
        distance);
}

void DemoRenderer::reset_camera() {
    _camera_distance = 5.0f;
    LUISA_INFO("[rbc_editor_py] reset_camera()");
}

}// namespace rbc::pyside_demo
