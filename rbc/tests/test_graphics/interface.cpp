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
#include <rbc_app/graphics_utils.h>
#include "generated/rbc_backend.h"
#include "rbc_graphics/object_types.h"
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
#include <tracy_wrapper.h>
#include <rbc_core/runtime_static.h>
#include <luisa/gui/window.h>

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>
struct ContextImpl : RBCStruct {
    luisa::fiber::scheduler scheduler;
    luisa::string backend = "dx";
    GraphicsUtils utils;
    vstd::optional<Window> window;
    uint2 window_size;
    double last_frame_time{};
    Clock clk;
    uint64_t frame_index{};
    ContextImpl() {
        log_level_info();
        RuntimeStaticBase::init_all();
    }
    ~ContextImpl() {
        utils.dispose();
        RuntimeStaticBase::dispose_all();
    }
};
void RBCContext::init_device(void *this_, luisa::string_view rhi_backend, luisa::string_view program_path, luisa::string_view shader_path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.backend = rhi_backend;
    c.utils.init_device(
        program_path,
        c.backend);
    c.utils.init_graphics(shader_path);
}

void RBCContext::init_render(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.init_render();
}
void RBCContext::load_skybox(void *this_, luisa::string_view path, uint2 size) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.render_plugin()->update_skybox(path, PixelStorage::FLOAT4, size);
}
void RBCContext::create_window(void *this_, luisa::string_view name, uint2 size, bool resiable) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.window.create(luisa::string{name}, size, resiable);
    c.utils.init_display(size, c.window->native_display(), c.window->native_handle());
    c.window_size = size;
    c.window->set_window_size_callback([&](uint2 size) {
        c.window_size = size;
    });
}
void *RBCContext::create_mesh(void *this_, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, luisa::span<std::byte> offset_uint32) {
    auto &c = *static_cast<ContextImpl *>(this_);
    auto ptr = new DeviceMesh();
    manually_add_ref(ptr);
    vstd::vector<uint32_t> vec;
    vec.push_back_uninitialized(offset_uint32.size() / sizeof(uint));
    std::memcpy(vec.data(), offset_uint32.data(), vec.size_bytes());
    c.utils.create_mesh(ptr, vertex_count, contained_normal, contained_tangent, uv_count, triangle_count, std::move(vec));
    return ptr;
}

void *RBCContext::load_mesh(void *this_, luisa::string_view file_path, uint64_t file_offset, uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, luisa::span<std::byte> offset_uint32) {
    auto &c = *static_cast<ContextImpl *>(this_);
    vstd::vector<uint32_t> vec;
    vec.push_back_uninitialized(offset_uint32.size() / sizeof(uint));
    std::memcpy(vec.data(), offset_uint32.data(), vec.size_bytes());
    auto ptr = new DeviceMesh();
    manually_add_ref(ptr);
    ptr->async_load_from_file(
        file_path,
        vertex_count,
        triangle_count,
        contained_normal,
        contained_tangent,
        uv_count,
        std::move(vec),
        false,
        true,
        file_offset,
        true);
    return ptr;
}

luisa::span<std::byte> RBCContext::get_mesh_data(void *this_, void *handle) {
    auto mesh = (DeviceMesh *)handle;
    mesh->wait_finished();
    auto host_data = mesh->host_data();
    return host_data;
}
void RBCContext::update_mesh(void *this_, void *handle, bool only_vertex) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.update_mesh_data((DeviceMesh *)handle, only_vertex);
}
void *RBCContext::create_pbr_material(void *this_) {
    auto ptr = new MaterialStub();
    manually_add_ref(ptr);
    ptr->craete_pbr_material();
    return ptr;
}
void RBCContext::update_material(void *this_, void *ptr, luisa::string_view json) {
    auto mat = static_cast<MaterialStub *>(ptr);
    mat->update_material(json);
}
luisa::string RBCContext::get_material_json(void *this_, void *mat_ptr) {
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
void *RBCContext::add_area_light(void *this_, luisa::float4x4 matrix, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = new LightStub{};
    manually_add_ref(stub);
    stub->add_area_light(matrix, luminance, visible);
    return stub;
}
void *RBCContext::add_disk_light(void *this_, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = new LightStub{};
    manually_add_ref(stub);
    stub->add_disk_light(center, radius, luminance, forward_dir, visible);
    return stub;
}
void *RBCContext::add_point_light(void *this_, luisa::float3 center, float radius, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = new LightStub{};
    manually_add_ref(stub);
    stub->add_point_light(center, radius, luminance, visible);
    return stub;
}
void *RBCContext::add_spot_light(void *this_, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = new LightStub{};
    manually_add_ref(stub);
    stub->add_spot_light(center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible);
    return stub;
}
void *RBCContext::create_texture(void *this_, rbc::LCPixelStorage storage, luisa::uint2 size, uint32_t mip_level) {
    auto &c = *static_cast<ContextImpl *>(this_);
    auto ptr = new DeviceImage();
    manually_add_ref(ptr);
    c.utils.create_texture(ptr, (PixelStorage)storage, size, mip_level);
    return ptr;
}
luisa::span<std::byte> RBCContext::get_texture_data(void *this_, void *handle) {
    auto mesh = (DeviceImage *)handle;
    mesh->wait_finished();
    auto host_data = mesh->host_data();
    return host_data;
}
void RBCContext::update_texture(void *this_, void *ptr) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.update_texture((DeviceImage *)ptr);
}
void *RBCContext::load_texture(void *this_, luisa::string_view file_path, uint64_t file_offset, rbc::LCPixelStorage storage, luisa::uint2 size, uint32_t mip_level, bool is_vt) {
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
uint RBCContext::texture_heap_idx(void *this_, void *ptr) {
    auto tex = (DeviceImage *)ptr;
    tex->wait_executed();
    return tex->heap_idx();
}
void RBCContext::update_area_light(void *this_, void *light, luisa::float4x4 matrix, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = reinterpret_cast<LightStub *>(light);
    stub->update_area_light(matrix, luminance, visible);
}
void RBCContext::update_disk_light(void *this_, void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = reinterpret_cast<LightStub *>(light);
    stub->update_disk_light(center, radius, luminance, forward_dir, visible);
}
void RBCContext::update_point_light(void *this_, void *light, luisa::float3 center, float radius, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = reinterpret_cast<LightStub *>(light);
    stub->update_point_light(center, radius, luminance, visible);
}
void RBCContext::update_spot_light(void *this_, void *light, luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    auto &render_device = RenderDevice::instance();
    auto stub = reinterpret_cast<LightStub *>(light);
    stub->update_spot_light(center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible);
}

void *RBCContext::create_object(void *this_, luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) {
    auto stub = new ObjectStub{};
    manually_add_ref(stub);
    stub->create_object(matrix, (DeviceMesh *)mesh, materials);
    return stub;
}
void RBCContext::update_object_pos(void *this_, void *ptr, luisa::float4x4 matrix) {
    auto stub = static_cast<ObjectStub *>(ptr);
    stub->update_object_pos(matrix);
}
void RBCContext::update_object(void *this_, void *ptr, luisa::float4x4 matrix, void *mesh, luisa::vector<RC<RCBase>> const &materials) {
    auto stub = static_cast<ObjectStub *>(ptr);
    stub->update_object(matrix, (DeviceMesh *)mesh, materials);
}
void RBCContext::reset_view(void *this_, luisa::uint2 resolution) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.resize_swapchain(resolution, c.window->native_display(), c.window->native_handle());
}
void RBCContext::reset_frame_index(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.frame_index = 0;
}
void RBCContext::set_view_camera(void *this_, luisa::float3 pos, float roll, float pitch, float yaw) {
    auto &c = *static_cast<ContextImpl *>(this_);
    auto &cam = c.utils.render_plugin()->get_camera(c.utils.default_pipe_ctx());
    cam.position = make_double3(pos);
    cam.rotation_roll = roll;
    cam.rotation_pitch = pitch;
    cam.rotation_yaw = yaw;
}
void RBCContext::disable_view(void *this_) {
    // TODO
}
bool RBCContext::should_close(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    return c.window->should_close();
}
void RBCContext::tick(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    RBCFrameMark;// Mark frame boundary

    RBCZoneScopedN("ContextImpl::tick");

    {
        RBCZoneScopedN("Poll Events");
        if (c.window)
            c.window->poll_events();
    }

    {
        RBCZoneScopedN("Update Camera");
        auto &cam = c.utils.render_plugin()->get_camera(c.utils.default_pipe_ctx());
        if (any(c.window_size != c.utils.dst_image().size())) {
            c.utils.resize_swapchain(c.window_size, c.window->native_display(), c.window->native_handle());
        }
        cam.aspect_ratio = (float)c.window_size.x / (float)c.window_size.y;
        auto time = c.clk.toc();
        auto delta_time = time - c.last_frame_time;
        RBCPlot("Frame Time (ms)", delta_time * 1000.0);
        c.last_frame_time = time;

        {
            RBCZoneScopedN("Render Tick");
            c.utils.tick(
                static_cast<float>(delta_time),
                c.frame_index,
                c.window_size);
        }

        ++c.frame_index;
        RBCPlot("Frame Index", static_cast<float>(c.frame_index));
    }
}
void *RBCContext::_create_() {
    auto ptr = new ContextImpl{};
    return ptr;
}
void RBCContext::_destroy_(void *ptr) {
    delete static_cast<ContextImpl *>(ptr);
}
