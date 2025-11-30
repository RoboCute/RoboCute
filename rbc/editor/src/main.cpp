#include <QApplication>
#include <QFile>
#include "RBCEditor/MainWindow.h"
#include "RBCEditor/EditorEngine.h"

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
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>

#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>

#include "generated/rbc_backend.h"
#include "object_types.h"
#include "RBCEditor/runtime/RenderUtils.h"
#include "RBCEditor/runtime/RenderScene.h"

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

struct ContextImpl : RBCContext {
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    GraphicsUtils utils;
    MatCode test_default_mat_code;
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
        material::OpenPBR mat{};
        mat.base.albedo = make_half3((half)0.5f, (half)0.5f, (half)0.5f);
        mat.specular.specular_color_and_rough.w = 0.5f;
        mat.specular.roughness_anisotropy_angle = 0.7f;
        // Make material instance
        auto &sm = SceneManager::instance();
        sm.mat_manager().emplace_mat_type<material::PolymorphicMaterial, material::OpenPBR>(sm.bindless_allocator(), 65536);
        test_default_mat_code = sm.mat_manager().emplace_mat_instance(
            mat,
            RenderDevice::instance().lc_main_cmd_list(),
            sm.bindless_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue(), material::PolymorphicMaterial::index<material::OpenPBR>);
    }

    void init_render() override {
        utils.init_render();
    }
    void load_skybox(luisa::string_view path, uint2 size) override {
        utils.render_plugin->update_skybox(path, size);
    }
    void create_window(luisa::string_view name, uint2 size, bool resiable) override {
        utils.init_display(name, size, resiable);
        window_size = size;
        utils.window->set_window_size_callback([&](uint2 size) {
            window_size = size;
        });
    }
    static uint64_t get_mesh_size(uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count) {
        uint64_t size = vertex_count * sizeof(float3);
        if (contained_normal) {
            size += vertex_count * sizeof(float3);
        }
        if (contained_tangent) {
            size += vertex_count * sizeof(float4);
        }
        size += uv_count * sizeof(float2);
        size += triangle_count * sizeof(Triangle);
        return size;
    }

    void *create_mesh(void *data, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count) override {
        auto mesh_size = get_mesh_size(vertex_count, contained_normal, contained_tangent, uv_count, triangle_count);
        luisa::BinaryBlob temp_blob{
            (std::byte *)data,
            mesh_size,
            {}};
        auto ptr = new DeviceMesh();
        RC<DeviceMesh>::manually_add_ref(ptr);
        ptr->async_load_from_memory(
            std::move(temp_blob),
            vertex_count,
            contained_normal,
            contained_tangent,
            uv_count,
            {},   // TODO: submesh & materials
            false,// only build BLAS while need ray-tracing
            true);
        ptr->wait_finished();
        return ptr;
    }
    void remove_mesh(void *handle) override {
        RC<DeviceMesh>::manually_release_ref(reinterpret_cast<DeviceMesh *>(handle));
    }
    void *add_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        RC<LightStub>::manually_add_ref(stub);
        stub->light_type = LightType::Area;
        stub->id = utils.lights->add_area_light(
            render_device.lc_main_cmd_list(),
            matrix,
            luminance,
            {}, {},
            visible);
        return stub;
    }
    void *add_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        RC<LightStub>::manually_add_ref(stub);
        stub->light_type = LightType::Disk;
        stub->id = utils.lights->add_disk_light(
            render_device.lc_main_cmd_list(),
            center,
            radius,
            luminance,
            forward_dir,
            visible);
        return stub;
    }
    void *add_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        RC<LightStub>::manually_add_ref(stub);
        stub->light_type = LightType::Sphere;
        stub->id = utils.lights->add_point_light(
            render_device.lc_main_cmd_list(),
            center,
            radius,
            luminance,
            visible);
        return stub;
    }
    void *add_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = new LightStub{};
        RC<LightStub>::manually_add_ref(stub);
        stub->light_type = LightType::Spot;
        stub->id = utils.lights->add_spot_light(
            render_device.lc_main_cmd_list(),
            center,
            radius,
            luminance,
            forward_dir,
            angle_atten_pow,
            small_angle_radians,
            angle_atten_pow,
            visible);
    }
    void remove_light(void *light) override {
        auto stub = reinterpret_cast<LightStub *>(light);

        RC<LightStub>::manually_release_ref(stub, [&] {
            auto id = stub->id;
            auto type = stub->light_type;
            switch (type) {
                case rbc::LightType::Sphere:
                    utils.lights->remove_point_light(id);
                    break;
                case rbc::LightType::Spot:
                    utils.lights->remove_spot_light(id);
                    break;
                case rbc::LightType::Area:
                    utils.lights->remove_area_light(id);
                    break;
                case rbc::LightType::Disk:
                    utils.lights->remove_disk_light(id);
                    break;
                case rbc::LightType::Blas:
                    utils.lights->remove_mesh_light(id);
                    break;
                default:
                    LUISA_ERROR("Unsupported light type.");
            }
        });
    }
    void update_area_light(void *light, luisa::float4x4 matrix, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        LUISA_ASSERT(stub->light_type == LightType::Area);
        utils.lights->update_area_light(
            render_device.lc_main_cmd_list(),
            stub->id,
            matrix,
            luminance,
            {}, {},
            visible);
    }
    void update_disk_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        LUISA_ASSERT(stub->light_type == LightType::Disk);
        utils.lights->update_disk_light(
            render_device.lc_main_cmd_list(),
            stub->id,
            center,
            radius,
            luminance,
            forward_dir,
            visible);
    }
    void update_point_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        LUISA_ASSERT(stub->light_type == LightType::Sphere);
        utils.lights->update_point_light(
            render_device.lc_main_cmd_list(),
            stub->id,
            center,
            radius,
            luminance,
            visible);
    }
    void update_spot_light(void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) override {
        auto &render_device = RenderDevice::instance();
        auto stub = reinterpret_cast<LightStub *>(light);
        LUISA_ASSERT(stub->light_type == LightType::Spot);
        utils.lights->update_spot_light(
            render_device.lc_main_cmd_list(),
            stub->id,
            center,
            radius,
            luminance,
            forward_dir,
            angle_radians,
            small_angle_radians,
            angle_atten_pow,
            visible);
    }
    void *create_object(luisa::float4x4 matrix, void *mesh) override {
        auto stub = new ObjectStub{};
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        stub->mesh_ref = reinterpret_cast<DeviceMesh *>(mesh);
        stub->mesh_ref->wait_finished();
        stub->tlas_idx = sm.accel_manager().emplace_mesh_instance(
            render_device.lc_main_cmd_list(),
            sm.host_upload_buffer(),
            sm.buffer_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue(),
            stub->mesh_ref->mesh_data(),
            {&test_default_mat_code, 1},//TODO :mat code
            matrix);
        return stub;
    }
    void update_object(luisa::float4x4 matrix) override {
        auto stub = new ObjectStub{};
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        sm.accel_manager().set_mesh_instance(
            render_device.lc_main_cmd_list(),
            sm.buffer_uploader(),
            stub->tlas_idx,
            matrix, 0xff, true);
    }
    void update_object(luisa::float4x4 matrix, void *mesh) override {
        auto stub = new ObjectStub{};
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        stub->mesh_ref = reinterpret_cast<DeviceMesh *>(mesh);
        stub->mesh_ref->wait_finished();
        sm.accel_manager().set_mesh_instance(
            stub->tlas_idx,
            render_device.lc_main_cmd_list(),
            sm.host_upload_buffer(),
            sm.buffer_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue(),
            stub->mesh_ref->mesh_data(),
            {&test_default_mat_code, 1},// TODO: mat_code
            matrix);
    }
    void remove_object(void *object_ptr) override {
        auto stub = reinterpret_cast<ObjectStub *>(object_ptr);
        RC<ObjectStub>::manually_release_ref(stub, [&] {
            auto &render_device = RenderDevice::instance();
            auto &sm = SceneManager::instance();
            sm.accel_manager().remove_mesh_instance(
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                stub->tlas_idx);
        });
    }
    void reset_view(luisa::uint2 resolution) override {
        utils.resize_swapchain(resolution);
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
        utils.tick([&]() {
            auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
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
        });
    }
};
RBCContext *RBCContext::_create_() {
    return new ContextImpl{};
}
int main(int argc, char **argv) {
    int ret = 0;
    log_level_info();
    // QApplication app(argc, argv);
    // QFile f(":/main.qss");
    // QString styleSheet = "";
    // if (f.open(QIODevice::ReadOnly)) {
    //     styleSheet = f.readAll();
    // }
    // app.setStyleSheet(styleSheet);
    // rbc::EditorEngine::instance().init(argc, argv);
    // {
    //     MainWindow window;
    //     window.setupUi();
    //     window.startSceneSync("http://127.0.0.1:5555");
    //     window.show();
    //     ret = QApplication::exec();
    // }
    // rbc::EditorEngine::instance().shutdown();

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
    vstd::optional<rbc::SimpleScene> simple_scene;
    simple_scene.create(*utils.lights);
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
    auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
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

    return ret;
}
