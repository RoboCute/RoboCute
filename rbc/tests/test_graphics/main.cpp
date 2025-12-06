#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include "simple_scene.h"
#include <rbc_app/graphics_utils.h>
#include "generated/rbc_backend.h"
#include "rbc_app/graphics_object_types.h"
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>
struct ContextImpl : RBCContext {
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    GraphicsUtils utils;
    uint2 window_size;
    double last_frame_time{};
    Clock clk;
    uint64_t frame_index{};
    ContextImpl() {
        log_level_info();
    }
    void dispose() override {
        delete this;
    }
    ~ContextImpl() {
        utils.dispose();
    }
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
        utils.render_plugin->update_skybox(path, size);
    }
    void create_window(luisa::string_view name, uint2 size, bool resiable) override {
        utils.init_display_with_window(name, size, resiable);
        window_size = size;
        utils.window->set_window_size_callback([&](uint2 size) {
            window_size = size;
        });
    }

    void *create_mesh(uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, luisa::span<std::byte> offset_uint32) override {
        auto ptr = new DeviceMesh();
        manually_add_ref(ptr);
        vstd::vector<uint32_t> vec;
        vec.push_back_uninitialized(offset_uint32.size() / sizeof(uint));
        std::memcpy(vec.data(), offset_uint32.data(), vec.size_bytes());
        utils.create_mesh(ptr, vertex_count, contained_normal, contained_tangent, uv_count, triangle_count, std::move(vec));
        return ptr;
    }

    void *load_mesh(luisa::string_view file_path, uint64_t file_offset, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, luisa::span<std::byte> offset_uint32) override {

        vstd::vector<uint32_t> vec;
        vec.push_back_uninitialized(offset_uint32.size() / sizeof(uint));
        std::memcpy(vec.data(), offset_uint32.data(), vec.size_bytes());
        auto ptr = new DeviceMesh();
        manually_add_ref(ptr);
        ptr->async_load_from_file(
            file_path,
            file_offset,
            contained_normal,
            contained_tangent,
            uv_count,
            std::move(vec),
            false,
            false,
            file_offset,
            ~0ull,
            true);
        ptr->calculate_bounding_box();
        return ptr;
    }

    luisa::span<std::byte> get_mesh_data(void *handle) override {
        auto mesh = (DeviceMesh *)handle;
        mesh->wait_finished();
        auto host_data = mesh->host_data();
        return host_data;
    }
    void update_mesh(void *handle, bool only_vertex) override {
        utils.update_mesh_data((DeviceMesh *)handle, only_vertex);
    }
    void *create_pbr_material() override {
        auto ptr = new MaterialStub();
        manually_add_ref(ptr);
        ptr->craete_pbr_material();
        return ptr;
    }
    void update_material(void *ptr, luisa::string_view json) override {
        auto mat = static_cast<MaterialStub *>(ptr);
        mat->update_material(json);
    }
    luisa::string get_material_json(void *mat_ptr) override {
        auto ptr = (MaterialStub *)mat_ptr;
        JsonSerializer ser(false);
        ptr->mat_data.visit(
            [&](auto &a) {
                GraphicsUtils::openpbr_json_ser(ser, a);
            });
        auto data = ser.write_to();
        return luisa::string{
            luisa::string_view{(char const *)data.data(), data.size()}};
    }
    void *add_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        manually_add_ref(stub);
        stub->add_area_light(matrix, luminance, visible);
        return stub;
    }
    void *add_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        manually_add_ref(stub);
        stub->add_disk_light(center, radius, luminance, forward_dir, visible);
        return stub;
    }
    void *add_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        manually_add_ref(stub);
        stub->add_point_light(center, radius, luminance, visible);
        return stub;
    }
    void *add_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        manually_add_ref(stub);
        stub->add_spot_light(center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible);
        return stub;
    }
    void *create_texture(rbc::LCPixelStorage storage, luisa::uint2 size, uint32_t mip_level) override {
        auto ptr = new DeviceImage();
        manually_add_ref(ptr);
        utils.create_texture(ptr, (PixelStorage)storage, size, mip_level);
        return ptr;
    }
    luisa::span<std::byte> get_texture_data(void *handle) override {
        auto mesh = (DeviceImage *)handle;
        mesh->wait_finished();
        auto host_data = mesh->host_data();
        return host_data;
    }
    void update_texture(void *ptr) override {
        utils.update_texture((DeviceImage *)ptr);
    }
    void *load_texture(luisa::string_view file_path, uint64_t file_offset, rbc::LCPixelStorage storage, luisa::uint2 size, uint32_t mip_level, bool is_vt) override {
        auto &sm = SceneManager::instance();
        if (is_vt) {
            auto ptr = new DeviceSparseImage();
            manually_add_ref(ptr);
            ptr->load(
                &sm.tex_streamer(),
                {},
                file_path,
                file_offset,
                Sampler{},
                (PixelStorage)storage,
                size,
                mip_level);
            return ptr;
        } else {
            auto ptr = new DeviceImage();
            manually_add_ref(ptr);
            ptr->async_load_from_file(
                file_path,
                file_offset,
                Sampler{},
                (PixelStorage)storage,
                size,
                mip_level,
                DeviceImage::ImageType::Float,
                true);
            return ptr;
        }
    }
    uint texture_heap_idx(void *ptr) override {
        auto tex = (DeviceImage *)ptr;
        tex->wait_executed();
        return tex->heap_idx();
    }
    void update_area_light(void *light, luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        stub->update_area_light(matrix, luminance, visible);
    }
    void update_disk_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        stub->update_disk_light(center, radius, luminance, forward_dir, visible);
    }
    void update_point_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        stub->update_point_light(center, radius, luminance, visible);
    }
    void update_spot_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        stub->update_spot_light(center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible);
    }

    void *create_object(luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) override {
        auto stub = new ObjectStub{};
        manually_add_ref(stub);
        stub->create_object(matrix, (DeviceMesh *)mesh, materials);
        return stub;
    }
    void update_object_pos(void *ptr, luisa::float4x4 matrix) override {
        auto stub = static_cast<ObjectStub *>(ptr);
        stub->update_object_pos(matrix);
    }
    void update_object(void *ptr, luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) override {
        auto stub = static_cast<ObjectStub *>(ptr);
        stub->update_object(matrix, (DeviceMesh *)mesh, materials);
    }
    void reset_view(luisa::uint2 resolution) override {
        utils.resize_swapchain(resolution);
    }
    void reset_frame_index() override {
        frame_index = 0;
    }
    void set_view_camera(luisa::float3 pos, float roll, float pitch, float yaw) override {
        auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
        cam.position = make_double3(pos);
        cam.rotation_roll = roll;
        cam.rotation_pitch = pitch;
        cam.rotation_yaw = yaw;
    }
    void disable_view() override {
        // TODO
    }
    bool should_close() override {
        return utils.should_close();
    }
    void tick() override {
        if (utils.window)
            utils.window->poll_events();
        auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
        if (any(window_size != utils.dst_image.size())) {
            utils.resize_swapchain(window_size);
        }
        cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
        auto time = clk.toc();
        auto delta_time = time - last_frame_time;
        last_frame_time = time;

        utils.tick(
            static_cast<float>(delta_time),
            frame_index,
            window_size);
        ++frame_index;
    }
};
RBCContext *RBCContext::_create_() {
    return new ContextImpl{};
}

#ifdef STANDALONE
int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
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
    utils.init_display_with_window("test_graphics", uint2(1024), true);
    uint64_t frame_index = 0;
    // Present is ping-pong frame-buffer and compute is triple-buffer
    Clock clk;
    double last_frame_time = 0;
    vstd::optional<SimpleScene> simple_scene;
    simple_scene.create(*utils.lights);
    // Test FOV
    vstd::optional<float3> cube_move, light_move;
    bool reset = false;
    auto &click_mng = utils.render_settings.read_mut<ClickManager>();
    uint2 window_size = utils.window->size();
    float2 start_uv, end_uv;
    bool dragging = false;
    utils.window->set_mouse_callback([&](MouseButton button, Action action, float2 xy) {
        if (action == Action::ACTION_PRESSED) {
            start_uv = clamp(xy / make_float2(window_size), float2(0.f), float2(1.f));
            dragging = true;
        } else if (action == Action::ACTION_RELEASED) {
            dragging = false;
        }
    });
    utils.window->set_cursor_position_callback([&](float2 xy) {
        if (dragging) {
            end_uv = clamp(xy / make_float2(window_size), float2(0.f), float2(1.f));
        }
    });
    utils.window->set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        if (action == Action::ACTION_PRESSED) return;
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
    utils.window->set_window_size_callback([&](uint2 size) {
        window_size = size;
    });
    auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
    cam.fov = radians(80.f);
    while (!utils.should_close()) {
        if (reset) {
            reset = false;
            utils.reset_frame();
        }
        if (utils.window)
            utils.window->poll_events();
        auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
        if (any(window_size != utils.dst_image.size())) {
            utils.resize_swapchain(window_size);
            frame_index = 0;
        }
        cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
        auto time = clk.toc();
        auto delta_time = time - last_frame_time;
        last_frame_time = time;
        // scene logic
        if (cube_move) {
            simple_scene->move_cube(*cube_move);
            cube_move.destroy();
        }
        if (light_move) {
            simple_scene->move_light(*light_move);
            light_move.destroy();
        }
        click_mng.add_frame_selection("dragging", min(start_uv, end_uv) * 2.f - 1.f, max(start_uv, end_uv) * 2.f - 1.f);
        auto dragging_result = click_mng.query_frame_selection("dragging");
        if (!dragging_result.empty()) {
            click_mng.set_contour_objects(std::move(dragging_result));
        }

        auto tick_stage = GraphicsUtils::TickStage::RasterPreview;
        // const uint sample = 16;
        // if (frame_index > sample) {
        //     tick_stage = GraphicsUtils::TickStage::PresentOfflineResult;
        // }
        utils.tick(
            static_cast<float>(delta_time),
            frame_index,
            window_size,
            tick_stage);
        // if (frame_index == sample) {
        //     LUISA_INFO("Denoising..");
        //     utils.denoise();
        // }
        ++frame_index;
        auto click_result = click_mng.query_result("click");
        if (click_result) {
            MatCode m{click_result->mat_code};
            LUISA_INFO("Clicked:\ntriangle_bary: {}\ninst_id: {}\nprim_id: {}\nmat_type: {}\nmat_idx: {}\nsubmesh_index: {}",
                       click_result->triangle_bary,
                       click_result->inst_id,
                       click_result->prim_id,
                       m.get_type(),
                       m.get_inst_id(),
                       click_result->submesh_index);
        }
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
#endif
