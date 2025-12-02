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
#include "utils.h"
#include "generated/rbc_backend.h"
#include "object_types.h"
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
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
        // TODO: make material for test
        material::OpenPBR mat{};
        mat.base.albedo = {0.5f, 0.5f, 0.5f};
        mat.specular.roughness = 0.5f;
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

    void *create_mesh(luisa::span<std::byte> data, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, luisa::span<std::byte> offset_uint32) override {
        auto mesh_size = get_mesh_size(vertex_count, contained_normal, contained_tangent, uv_count, triangle_count);
        if (mesh_size != data.size_bytes()) [[unlikely]] {
            LUISA_ERROR("Mesh desired size {} unmatch to buffer size {}", mesh_size, data.size_bytes());
        }
        luisa::BinaryBlob temp_blob{
            data.data(),
            mesh_size,
            {}};
        auto ptr = new DeviceMesh();
        RC<DeviceMesh>::manually_add_ref(ptr);
        vstd::vector<uint32_t> vec;
        vec.push_back_uninitialized(offset_uint32.size() / sizeof(uint));
        std::memcpy(vec.data(), offset_uint32.data(), vec.size_bytes());
        ptr->async_load_from_memory(
            std::move(temp_blob),
            vertex_count,
            contained_normal,
            contained_tangent,
            uv_count,
            std::move(vec),
            false,// only build BLAS while need ray-tracing
            true,
            true);
        return ptr;
    }

    luisa::span<std::byte> get_mesh_data(void *handle) override {
        auto mesh = (DeviceMesh *)handle;
        mesh->wait_finished();
        auto host_data = mesh->host_data();
        return host_data;
    }
    void *create_pbr_material(luisa::string_view json) override {
        auto ptr = new MaterialStub();
        ptr->mat_data.reset_as(MaterialStub::MatDataType::IndexOf<material::OpenPBR>);
        JsonDeSerializer deser(json);
        RC<MaterialStub>::manually_add_ref(ptr);
        auto &mat_data = ptr->mat_data.force_get<material::OpenPBR>();
        GraphicsUtils::openpbr_json_deser(
            deser,
            mat_data);
        auto &sm = SceneManager::instance();
        auto &render_device = RenderDevice::instance();
        ptr->mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial, material::OpenPBR>(
            mat_data,
            render_device.lc_main_cmd_list(),
            sm.bindless_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue());
        return ptr;
    }
    void update_pbr_material(void *ptr, luisa::string_view json) override {
        auto mat = static_cast<MaterialStub *>(ptr);
        if (mat->mat_code.get_type() != material::PolymorphicMaterial::index<material::OpenPBR>) [[unlikely]] {
            LUISA_ERROR("Material type mismatch.");
        }
        JsonDeSerializer deser(json);
        RC<MaterialStub>::manually_add_ref(mat);
        auto &mat_data = mat->mat_data.force_get<material::OpenPBR>();
        GraphicsUtils::openpbr_json_deser(
            deser,
            mat_data);
        auto &sm = SceneManager::instance();
        sm.mat_manager().set_mat_instance(
            mat->mat_code,
            sm.buffer_uploader(),
            {(std::byte const *)&mat_data,
             sizeof(mat_data)});
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
        return stub;
    }
    void *create_texture(luisa::span<std::byte> data, rbc::LCPixelStorage storage, luisa::uint2 size, uint32_t mip_level) override {
        size_t size_bytes = 0;
        for (auto i : vstd::range(mip_level))
            size_bytes += pixel_storage_size((PixelStorage)storage, make_uint3(size >> (uint)i, 1u));
        if (size_bytes != data.size()) [[unlikely]] {
            LUISA_ERROR("Texture desired size {} unmatch to buffer size {}", size_bytes, data.size_bytes());
        }
        auto ptr = new DeviceImage();
        RC<DeviceImage>::manually_add_ref(ptr);
        ptr->async_load_from_memory(
            luisa::BinaryBlob(data.data(), data.size(), {}),
            Sampler{},
            (PixelStorage)storage,
            size,
            mip_level,
            DeviceImage::ImageType::Float,
            true);
        return ptr;
    }
    uint texture_heap_idx(void *ptr) override {
        auto tex = (DeviceImage *)ptr;
        tex->wait_executed();
        return tex->heap_idx();
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
    static bool material_is_emission(luisa::span<RC<MaterialStub> const> materials) {
        bool contained_emission = false;
        for (auto &i : materials) {
            static_cast<MaterialStub *>(i.get())->mat_data.visit([&]<typename T>(T const &t) {
                if constexpr (std::is_same_v<T, material::OpenPBR>) {
                    for (auto &i : t.emission.luminance) {
                        if (i > 1e-3f) {
                            contained_emission = true;
                            return;
                        }
                    }
                } else {
                    for (auto &i : t.color) {
                        if (i > 1e-3f) {
                            contained_emission = true;
                            return;
                        }
                    }
                }
            });
        }
        return contained_emission;
    };
    void *create_object(luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) override {
        auto stub = new ObjectStub{};
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        stub->mesh_ref = reinterpret_cast<DeviceMesh *>(mesh);
        vstd::push_back_func(
            stub->materials,
            materials.size(),
            [&](size_t i) {
                return static_cast<MaterialStub *>(materials[i].get());
            });
        vstd::push_back_func(
            stub->material_codes,
            stub->materials.size(),
            [&](size_t i) {
                return stub->materials[i]->mat_code;
            });
        stub->mesh_ref->wait_finished();
        auto submesh_size = std::max<size_t>(1, stub->mesh_ref->mesh_data()->submesh_offset.size());
        if (!(materials.size() == submesh_size)) [[unlikely]] {
            LUISA_ERROR("Submesh size {} mismatch with material size {}", submesh_size, materials.size());
        }
        if (material_is_emission(
                {reinterpret_cast<RC<MaterialStub> const *>(materials.data()),
                 materials.size()})) {
            stub->mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                render_device.lc_main_cmd_list(),
                stub->mesh_ref,
                matrix,
                stub->material_codes);
            stub->type = ObjectRenderType::EmissionMesh;
        } else {
            stub->mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                render_device.lc_main_cmd_list(),
                sm.host_upload_buffer(),
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue(),
                stub->mesh_ref->mesh_data(),
                stub->material_codes,
                matrix);
            stub->type = ObjectRenderType::Mesh;
        }
        return stub;
    }
    void update_object_pos(void *ptr, luisa::float4x4 matrix) override {
        auto stub = static_cast<ObjectStub *>(ptr);
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        switch (stub->type) {
            case ObjectRenderType::Mesh: {
                sm.accel_manager().set_mesh_instance(
                    render_device.lc_main_cmd_list(),
                    sm.buffer_uploader(),
                    stub->mesh_tlas_idx,
                    matrix, 0xff, true);
            } break;
            case ObjectRenderType::EmissionMesh:
                utils.lights->update_mesh_light_sync(
                    render_device.lc_main_cmd_list(),
                    stub->mesh_light_idx,
                    matrix,
                    stub->material_codes);
                break;
            case ObjectRenderType::Procedural:
                sm.accel_manager().set_procedural_instance(
                    stub->procedural_idx,
                    matrix,
                    0xffu,
                    true);
                break;
        }
    }
    void update_object(void *ptr, luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) override {
        auto stub = static_cast<ObjectStub *>(ptr);
        RC<ObjectStub>::manually_add_ref(stub);
        auto &render_device = RenderDevice::instance();
        auto &sm = SceneManager::instance();
        stub->mesh_ref = reinterpret_cast<DeviceMesh *>(mesh);
        stub->materials.clear();
        stub->material_codes.clear();
        auto submesh_size = std::max<size_t>(1, stub->mesh_ref->mesh_data()->submesh_offset.size());
        if (!(materials.size() == submesh_size)) [[unlikely]] {
            LUISA_ERROR("Submesh size {} mismatch with material size {}", submesh_size, materials.size());
        }
        vstd::push_back_func(
            stub->materials,
            materials.size(),
            [&](size_t i) {
                return static_cast<MaterialStub *>(materials[i].get());
            });
        vstd::push_back_func(
            stub->material_codes,
            stub->materials.size(),
            [&](size_t i) {
                return stub->materials[i]->mat_code;
            });
        stub->mesh_ref->wait_finished();
        // TODO: change light type
        bool is_emission = material_is_emission(stub->materials);
        switch (stub->type) {
            case ObjectRenderType::Mesh:
                if (is_emission) {
                    sm.accel_manager().remove_mesh_instance(
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        stub->mesh_tlas_idx);
                    stub->mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                        render_device.lc_main_cmd_list(),
                        stub->mesh_ref,
                        matrix,
                        stub->material_codes);
                    stub->type = ObjectRenderType::EmissionMesh;
                } else {
                    sm.accel_manager().set_mesh_instance(
                        stub->mesh_tlas_idx,
                        render_device.lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        stub->mesh_ref->mesh_data(),
                        stub->material_codes,
                        matrix);
                }
                break;
            case ObjectRenderType::EmissionMesh:
                if (!is_emission) {
                    Lights::instance()->remove_mesh_light(stub->mesh_light_idx);
                    stub->mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                        render_device.lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        stub->mesh_ref->mesh_data(),
                        stub->material_codes,
                        matrix);
                    stub->type = ObjectRenderType::Mesh;
                } else {
                    utils.lights->update_mesh_light_sync(
                        render_device.lc_main_cmd_list(),
                        stub->mesh_light_idx,
                        matrix,
                        stub->material_codes,
                        &stub->mesh_ref);
                }
                break;
            case ObjectRenderType::Procedural: {
                LUISA_ERROR("Procedural not supported.");
            } break;
        }
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

#ifdef STANDALONE
int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    // ContextImpl impl{};
    // impl.init_device(
    //     "vk",
    //     "D:/RoboCute/build/windows/x64/debug",
    //     "D:/RoboCute/build/windows/x64/shader_build_vk");
    // impl.init_render();
    // impl.load_skybox("../sky.bytes", uint2(4096, 2048));
    // impl.create_window("py_window", uint2(1920, 1080), true);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

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
}
#endif
