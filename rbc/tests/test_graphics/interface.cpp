#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include "simple_scene.h"
#include <rbc_app/graphics_utils.h>
#include "generated/rbc_backend.h"
#include "rbc_graphics/object_types.h"
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
        utils.render_plugin()->update_skybox(path, size);
    }
    void create_window(luisa::string_view name, uint2 size, bool resiable) override {
        utils.init_display_with_window(name, size, resiable);
        window_size = size;
        utils.window()->set_window_size_callback([&](uint2 size) {
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
                MaterialStub::openpbr_json_ser(ser, a);
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
        auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
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
        if (utils.window())
            utils.window()->poll_events();
        auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
        if (any(window_size != utils.dst_image().size())) {
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
