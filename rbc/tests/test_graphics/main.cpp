#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include "simple_scene.h"
#include "utils.h"
#include "generated/rbc_backend.h"
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#ifdef STANDALONE
int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    log_level_info();
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }
    GraphicsUtils utils;
    utils.init_device(
        argv[0],
        backend.c_str());
    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name));
    utils.init_render();
    utils.render_plugin->update_skybox("../sky.bytes", uint2(4096, 2048));
    utils.init_display("test_graphics", uint2(1024), true);
    uint64_t frame_index = 0;
    // Present is ping-pong frame-buffer and compute is triple-buffer
    Clock clk;
    double last_frame_time = 0;
    vstd::optional<SimpleScene> simple_scene;
    simple_scene.create();
    // Test FOV
    vstd::optional<float3> cube_move, light_move;
    bool reset = false;
    utils.window->set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        if (action != Action::ACTION_PRESSED) return;
        frame_index = 0;
        reset = true;
        switch (key) {
            case Key::KEY_SPACE: {
                LUISA_INFO("Reset frame");
            } break;
            case Key::KEY_W: {
                light_move.create();
                *light_move += float3(0, 0.1, 0);
            } break;
            case Key::KEY_S: {
                light_move.create();
                *light_move += float3(0, -0.1, 0);
            } break;
            case Key::KEY_A: {
                light_move.create();
                *light_move += float3(-0.1, 0, 0);
            } break;
            case Key::KEY_D: {
                light_move.create();
                *light_move += float3(0.1, 0, 0);
            } break;
            case Key::KEY_Q: {
                light_move.create();
                *light_move += float3(0, 0, -0.1);
            } break;
            case Key::KEY_E: {
                light_move.create();
                *light_move += float3(0, 0, 0.1);
            } break;
            case Key::KEY_UP: {
                cube_move.create();
                *cube_move += float3(0, 0.1, 0);
            } break;
            case Key::KEY_DOWN: {
                cube_move.create();
                *cube_move += float3(0, -0.1, 0);
            } break;
            case Key::KEY_LEFT: {
                cube_move.create();
                *cube_move += float3(-0.1, 0, 0);
            } break;
            case Key::KEY_RIGHT: {
                cube_move.create();
                *cube_move += float3(0.1, 0, 0);
            } break;
        }
    });
    uint2 window_size = utils.window->size();
    utils.window->set_window_size_callback([&](uint2 size) {
        window_size = size;
    });
    auto& cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
    cam.fov = radians(80.f);
    while (!utils.should_close()) {
        if (reset) {
            reset = false;
            utils.reset_frame();
        }
        utils.tick([&]() {
            if (any(window_size != utils.dst_image.size())) {
                utils.resize_swapchain(window_size);
            }
            cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
            auto &frame_settings = utils.render_settings.read_mut<rbc::FrameSettings>();
            auto &dst_img = utils.dst_image;
            frame_settings.render_resolution = dst_img.size();
            frame_settings.display_resolution = dst_img.size();
            frame_settings.dst_img = &dst_img;
            auto time = clk.toc();
            auto delta_time = time - last_frame_time;
            last_frame_time = time;
            frame_settings.delta_time = (float)delta_time;
            frame_settings.time = time;
            frame_settings.frame_index = frame_index;
            ++frame_index;
            // scene logic
            if (cube_move) {
                simple_scene->move_cube(*cube_move);
                cube_move.destroy();
            }
            if (light_move) {
                simple_scene->move_light(*light_move);
                light_move.destroy();
            }
        });
    }
    // rpc_hook.shutdown_remote();
    utils.dispose([&]() {
        auto pipe_settings_json = utils.render_settings.serialize_to_json();
        if (pipe_settings_json.data()) {
            LUISA_INFO("{}", luisa::string_view{
                                 (char const *)pipe_settings_json.data(),
                                 pipe_settings_json.size()});
        }
        // destroy render-pipeline
        simple_scene.destroy();
    });
}
#else
struct ContextImpl : RBCContext {
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    GraphicsUtils utils;
    ContextImpl() {
        log_level_info();
    }
    void dispose() override {
        delete this;
    }
    ~ContextImpl() = default;
    void init_device(luisa::string_view rhi_backend, luisa::string_view program_path, luisa::string_view shader_path) override {
        backend = rhi_backend;
        utils.init_device(
            program_path,
            backend);
        utils.init_graphics(shader_path);
    }
    void init_render() override {
        utils.init_render();
    }
    void load_skybox(luisa::string_view path, uint2 size) override {
        utils.render_plugin->update_skybox("../sky.bytes", size);
    }
    void create_window(luisa::string_view name) override {
        // utils.init_display(name)
    }
    void add_external_window(uint64_t window_handle) override {}
    void *load_mesh(luisa::string_view path) override {
        return nullptr;
    }
    void *create_mesh(void *data, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count) override {
        return nullptr;
    }
    void remove_mesh(uint64_t handle) override {}
    void *add_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {
        return nullptr;
    }
    void *add_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {
        return nullptr;
    }
    void *add_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {
        return nullptr;
    }
    void *add_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_po, bool visible) override {
        return nullptr;
    }
    void remove_light(uint64_t handle) override {}
    void remove_light(void *light) override {}
    void update_area_light(void *light, luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {}
    void update_disk_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {}
    void update_point_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {}
    void update_spot_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_po, bool visible) override {}
    void *create_object(luisa::float4x4 matrix, void *mesh) override {
        return nullptr;
    }
    void update_object(luisa::float4x4 matrix) override {}
    void update_object(luisa::float4x4 matrix, void *mesh) override {}
    void remove_object(void *object_ptr) override {}
    void reset_view(luisa::uint2 resolution) override {}
    void set_view_camera(luisa::float3 pos, luisa::float3 forward_dir, luisa::float3 up_dir) override {}
    void disable_view() override {}
};
RBCContext *RBCContext::_create_() {
    return new ContextImpl{};
}

#endif