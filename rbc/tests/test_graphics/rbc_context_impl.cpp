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
#include <rbc_graphics/graphics_utils.h>
#include "generated/world.h"
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
#include <tracy_wrapper.h>
#include <rbc_core/runtime_static.h>
#include <luisa/gui/window.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world/entity.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/camera_component.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
#include <rbc_render/renderer_data.h>
using namespace luisa;
using namespace luisa::compute;
void save_image(luisa::filesystem::path const &path, Image<float> const &img);// implemented save_image.cpp

namespace rbc {
#include <material/mats.inl>
struct ContextImpl;
static ContextImpl *_ctx_inst{};
struct ContextImpl : RCBase {
    luisa::fiber::scheduler scheduler;
    GraphicsUtils utils;
    vstd::optional<Window> window;
    RC<world::Entity> display_cam_entity;
    uint2 window_size;
    ContextImpl() {
        if (_ctx_inst) [[unlikely]] {
            LUISA_ERROR("Context can only have one.");
        }
        _ctx_inst = this;
        log_level_info();
    }
    ~ContextImpl() {
        if (_ctx_inst == this) [[likely]]
            _ctx_inst = nullptr;
        utils.dispose();
    }
};

void RBCContext::init_world(void *this_, luisa::string_view meta_path, luisa::string_view binary_path) {
    rbc::world::init_world(meta_path, binary_path);
}
void RBCContext::init_device(void *this_, luisa::string_view rhi_backend, luisa::string_view program_path, luisa::string_view shader_path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.init_device(
        program_path,
        rhi_backend);
    c.utils.init_graphics(shader_path);
}

void RBCContext::init_render(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.utils.init_render();
}
void RBCContext::init_display(void *this_, luisa::string_view name, uint2 size, bool create_window, bool window_resizable) {
    auto &c = *static_cast<ContextImpl *>(this_);
    uint64_t native_display, native_handle;
    c.window_size = size;
    if (create_window && !c.window) {
        if (any(size == 0u)) [[unlikely]] {
            LUISA_ERROR("Size must be non-zero.");
        }
        c.window.create(luisa::string{name}, size, window_resizable);
        c.window->set_window_size_callback([&c](uint2 size) {
            c.window_size = size;
        });
        native_display = c.window->native_display();
        native_handle = c.window->native_handle();
    } else {
        native_display = invalid_resource_handle;
        native_handle = invalid_resource_handle;
    }
    c.utils.init_display(size, native_display, native_handle);
}
void RBCContext::reset_view(void *this_, luisa::uint2 resolution) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (c.window)
        c.utils.resize_swapchain(resolution, c.window->native_display(), c.window->native_handle());
    else
        c.utils.resize_swapchain(resolution, invalid_resource_handle, invalid_resource_handle);
}
void RBCContext::disable_view(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.window.destroy();
}
bool RBCContext::should_close(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (c.window)
        return c.window->should_close();
    return false;
}
void RBCContext::denoise(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (c.utils.denoise()) {
        c.utils.tick(
            GraphicsUtils::TickStage::PresentOfflineResult,
            true);
    }
}
void RBCContext::save_display_image_to(void *this_, luisa::string_view path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    auto &rd = RenderDevice::instance();
    if (!rd.lc_main_cmd_list().empty()) {
        rd.execute_before_cmdlist_commit_task();
        auto &stream = rbc::RenderDevice::instance().lc_main_stream();
        stream << rd.lc_main_cmd_list().commit();
    }
    save_image(path, c.utils.dst_image());
}
luisa::compute::TextureCreationInfo RBCContext::display_image(void *this_) {
    auto c = static_cast<ContextImpl *>(this_);
    luisa::compute::TextureCreationInfo r;
    auto &img = c->utils.dst_image();
    if (!img) {
        r.invalidate();
        return r;
    }
    r.handle = img.handle();
    r.native_handle = img.native_handle();
    r.format = img.format();
    r.dimension = 2;
    r.width = img.size().x;
    r.height = img.size().y;
    r.depth = 1;
    r.mipmap_levels = img.mip_levels();
    return r;
}
void RBCContext::tick(void *this_, rbc::TickStage tick_stage, bool prepare_denoise) {
    auto &c = *static_cast<ContextImpl *>(this_);
    RBCFrameMark;// Mark frame boundary

    RBCZoneScopedN("ContextImpl::tick");

    if (c.window) {
        RBCZoneScopedN("Poll Events");
        c.window->poll_events();
        if (c.utils.dst_image() && any(c.window_size != c.utils.dst_image().size())) {
            reset_view(this_, c.window_size);
        }
    }
    {
        RBCZoneScopedN("Update Camera");
        {
            RBCZoneScopedN("Render Tick");
            c.utils.tick(
                static_cast<GraphicsUtils::TickStage>(tick_stage),
                prepare_denoise);
        }
    }
}
void *RBCContext::create_display_cam(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (any(c.window_size == 0u) || !c.utils.dst_image()) [[unlikely]] {
        LUISA_ERROR("Display not initialized.");
    }
    if (!c.display_cam_entity) {
        c.display_cam_entity = world::create_object<world::Entity>();
        c.display_cam_entity->add_component<world::TransformComponent>();
        c.display_cam_entity->add_component<world::CameraComponent>();
    }
    auto ptr = c.display_cam_entity->get_component<world::CameraComponent>();
    if (!ptr) [[unlikely]] {
        ptr = c.display_cam_entity->add_component<world::CameraComponent>();
    }
    manually_add_ref(ptr);
    return ptr;
}
void RBCContext::clear_geometry_export_buffer(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (!c.display_cam_entity) [[unlikely]] {
        LUISA_ERROR("Display camera uninitialized.");
    }
    auto cam = c.display_cam_entity->get_component<world::CameraComponent>();
    if (!cam) [[unlikely]] {
        LUISA_ERROR("Display camera uninitialized.");
    }
    auto &map = c.utils.render_settings((RenderPlugin::PipeCtxStub *)cam->render_pipe_ctx());
    auto &s = map.read_mut<FrameSettings>();
    s.geometry_channel = GeometryType::NONE;
    s.pt_geometry_buffer = {};
}
void RBCContext::set_geometry_export_buffer(void *this_, luisa::compute::BufferCreationInfoInterop buffer, rbc::RendererGeometryType channel_type) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (!c.display_cam_entity) [[unlikely]] {
        LUISA_ERROR("Display camera uninitialized.");
    }
    auto cam = c.display_cam_entity->get_component<world::CameraComponent>();
    if (!cam) [[unlikely]] {
        LUISA_ERROR("Display camera uninitialized.");
    }
    auto &map = c.utils.render_settings((RenderPlugin::PipeCtxStub *)cam->render_pipe_ctx());
    auto &s = map.read_mut<FrameSettings>();
    s.geometry_channel = (rbc::GeometryType)channel_type;
    s.pt_geometry_buffer =
        (buffer.native_handle == 0 ||
         buffer.handle == invalid_resource_handle) ?
            BufferView<float>{} :
            BufferView<float>(
                buffer.native_handle,
                buffer.handle,
                sizeof(float),
                0,
                buffer.total_size_bytes / sizeof(float),
                buffer.total_size_bytes / sizeof(float));
}
void RBCContext::destroy_display_cam(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.display_cam_entity.reset();
}

void *RBCContext::_create_() {
    LUISA_ASSERT(!_ctx_inst);
    rbc::RuntimeStaticBase::init_all();
    rbc::PluginManager::init();
    auto ptr = new ContextImpl{};
    manually_add_ref(ptr);
    return ptr;
}
}// namespace rbc