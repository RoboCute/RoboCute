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
#include "generated/rbc_backend.h"
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
#include <tracy_wrapper.h>
#include <rbc_core/runtime_static.h>
#include <luisa/gui/window.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world/base_object.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
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
    RenderPlugin::PipeCtxStub *pipe_ctx{};
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
void RBCContext::create_window(void *this_, luisa::string_view name, uint2 size, bool resiable) {
    auto &c = *static_cast<ContextImpl *>(this_);
    c.window.create(luisa::string{name}, size, resiable);
    c.utils.init_display(size, c.window->native_display(), c.window->native_handle());
    if (!c.pipe_ctx) {
        c.pipe_ctx = c.utils.register_render_pipectx();
    }
}
void RBCContext::reset_view(void *this_, luisa::uint2 resolution) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (c.window)
        c.utils.resize_swapchain(resolution, c.window->native_display(), c.window->native_handle());
    else
        c.utils.resize_swapchain(resolution, invalid_resource_handle, invalid_resource_handle);
}
void RBCContext::set_view_camera(void *this_, luisa::float3 pos, float roll, float pitch, float yaw) {
    auto &c = *static_cast<ContextImpl *>(this_);
    if (!c.pipe_ctx) {
        c.pipe_ctx = c.utils.register_render_pipectx();
    }
    auto &cam = c.utils.render_settings(c.pipe_ctx).read_mut<Camera>();
    cam.position = make_double3(pos);
    cam.rotation_roll = roll;
    cam.rotation_pitch = pitch;
    cam.rotation_yaw = yaw;
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
            0,
            c.utils.dst_image().size(),
            GraphicsUtils::TickStage::PresentOfflineResult,
            true);
    }
}
void RBCContext::save_display_image_to(void *this_, luisa::string_view path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    save_image(path, c.utils.dst_image());
}
void RBCContext::tick(void *this_, float delta_time, luisa::uint2 resolution, uint32_t frame_index, rbc::TickStage tick_stage, bool prepare_denoise) {
    auto &c = *static_cast<ContextImpl *>(this_);
    RBCFrameMark;// Mark frame boundary

    RBCZoneScopedN("ContextImpl::tick");

    if (c.window) {
        RBCZoneScopedN("Poll Events");
        c.window->poll_events();
    }
    StateMap *render_settings{};
    if (c.pipe_ctx) {
        render_settings = &c.utils.render_settings(c.pipe_ctx);
    }
    {
        RBCZoneScopedN("Update Camera");
        if (render_settings) {
            auto &cam = render_settings->read_mut<Camera>();
            if (any(resolution != c.utils.dst_image().size())) {
                reset_view(this_, resolution);
            }
            cam.aspect_ratio = (float)resolution.x / (float)resolution.y;
        }
        RBCPlot("Frame Time (ms)", delta_time * 1000.0);
        if (render_settings) {
            auto &frame_settings = render_settings->read_mut<FrameSettings>();
            frame_settings.frame_index = frame_index;
        }
        {
            RBCZoneScopedN("Render Tick");
            c.utils.tick(
                static_cast<float>(delta_time),
                resolution,
                static_cast<GraphicsUtils::TickStage>(tick_stage),
                prepare_denoise);
        }

        ++frame_index;
        RBCPlot("Frame Index", static_cast<float>(frame_index));
    }
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