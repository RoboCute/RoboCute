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
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/camera_component.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
#include <rbc_render/renderer_data.h>
#include <rbc_app/camera_controller.h>
#include <luisa/runtime/buffer.h>
#include <rbc_world/callback_serializer.h>
using namespace luisa;
using namespace luisa::compute;
void save_image(luisa::filesystem::path const &path, Image<float> const &img);// implemented save_image.cpp

namespace rbc {
#include <material/mats.inl>
struct ContextImpl;
static ContextImpl *_ctx_inst{};
struct ContextImpl : RCBase {
    luisa::spin_mutex _ctx_mtx;
    luisa::fiber::scheduler scheduler;
    CameraController::Input camera_input{};
    vstd::unique_ptr<GraphicsUtils> utils;
    vstd::unique_ptr<Window> window;
    vstd::unique_ptr<CameraController> cam_controller;
    // vstd::unique_ptr<Ca
    RC<world::Entity> display_cam_entity;
    uint2 window_size;
    void clear_window_event() {
        if (!window) return;
        window->set_mouse_callback({});
        window->set_cursor_position_callback({});
        window->set_key_callback({});
    }
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
        utils.reset();
    }
};
void RBCContext::init_world(void *this_, luisa::string_view meta_path, luisa::string_view binary_path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    rbc::world::init_world(meta_path, binary_path);
}
void RBCContext::init_device(void *this_, luisa::string_view rhi_backend, luisa::string_view program_path, luisa::string_view shader_path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    c.utils = vstd::make_unique<GraphicsUtils>();
    c.utils->init_device(
        program_path,
        rhi_backend);
    c.utils->init_graphics(shader_path);
}

void RBCContext::init_render(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    c.utils->init_render();
}
void RBCContext::init_display(void *this_, luisa::string_view name, uint2 size, bool create_window, bool window_resizable) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    uint64_t native_display, native_handle;
    c.window_size = size;
    if (create_window && !c.window) {
        if (any(size == 0u)) [[unlikely]] {
            LUISA_ERROR("Size must be non-zero.");
        }
        c.window = vstd::make_unique<Window>(luisa::string{name}, size, window_resizable);
        c.window->set_window_size_callback([&c](uint2 size) {
            c.window_size = size;
        });
        native_display = c.window->native_display();
        native_handle = c.window->native_handle();
    } else {
        native_display = invalid_resource_handle;
        native_handle = invalid_resource_handle;
    }
    c.utils->init_display(size, native_display, native_handle);
}
void RBCContext::reset_view(void *this_, luisa::uint2 resolution) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (c.window)
        c.utils->resize_swapchain(resolution, c.window->native_display(), c.window->native_handle());
    else
        c.utils->resize_swapchain(resolution, invalid_resource_handle, invalid_resource_handle);
}
void RBCContext::disable_view(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    c.window.reset();
}
bool RBCContext::should_close(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (c.window)
        return c.window->should_close();
    return false;
}
void RBCContext::denoise(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (c.utils->denoise()) {
        c.utils->tick(
            GraphicsUtils::TickStage::PresentOfflineResult,
            true);
    }
}
void RBCContext::save_display_image_to(void *this_, luisa::string_view path) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    auto &rd = RenderDevice::instance();
    if (!rd.lc_main_cmd_list().empty()) {
        rd.execute_before_cmdlist_commit_task();
        auto &stream = rbc::RenderDevice::instance().lc_main_stream();
        stream << rd.lc_main_cmd_list().commit();
    }
    save_image(path, c.utils->dst_image());
}
luisa::compute::TextureCreationInfo RBCContext::display_image(void *this_) {
    auto c = static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c->_ctx_mtx};
    luisa::compute::TextureCreationInfo r;
    auto &img = c->utils->dst_image();
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
bool RBCContext::tick(void *this_, float delta_time, rbc::TickStage tick_stage, bool prepare_denoise) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::unique_lock lck{c._ctx_mtx};
    RBCFrameMark;// Mark frame boundary
    bool any_changed{false};
    RBCZoneScopedN("ContextImpl::tick");

    if (c.window) {
        RBCZoneScopedN("Poll Events");
        c.window->poll_events();
        if (c.utils->dst_image() && any(c.window_size != c.utils->dst_image().size())) {
            reset_view(this_, c.window_size);
            any_changed = true;
        }
        if (c.cam_controller) {
            c.camera_input.viewport_size = make_float2(c.window_size);
            c.cam_controller->grab_input_from_viewport(c.camera_input, delta_time);
            any_changed = c.cam_controller->any_changed();
        }
    }
    {
        RBCZoneScopedN("Update Camera");
        {
            lck.unlock();
            RBCZoneScopedN("Render Tick");
            c.utils->tick(
                static_cast<GraphicsUtils::TickStage>(tick_stage),
                prepare_denoise);
        }
    }
    return any_changed;
}
void *RBCContext::create_display_cam(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (any(c.window_size == 0u) || !c.utils->dst_image()) [[unlikely]] {
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
    return ptr;
}

void RBCContext::destroy_display_cam(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    c.clear_window_event();
    c.cam_controller.reset();
    c.display_cam_entity.reset();
}
void RBCContext::enable_camera_control(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (c.cam_controller) return;
    if (!c.window) [[unlikely]] {
        LUISA_ERROR("Window instance required for camera control.");
    }
    world::CameraComponent *cam_comp{};
    if (!c.display_cam_entity || ![&] {cam_comp = c.display_cam_entity->get_component<world::CameraComponent>(); return cam_comp; }()) [[unlikely]] {
        LUISA_ERROR("Display camera instance required for camera control.");
    }
    c.cam_controller = vstd::make_unique<CameraController>();
    auto &render_settings = c.utils->render_settings(static_cast<RenderPlugin::PipeCtxStub *>(cam_comp->render_pipe_ctx()));
    auto &cam = render_settings.read_mut<Camera>();
    c.cam_controller->camera = &cam;
    c.cam_controller->transform = c.display_cam_entity->get_component<world::TransformComponent>();
    c.window->set_mouse_callback([this_](MouseButton button, Action action, float2 xy) {
        auto &c = *static_cast<ContextImpl *>(this_);
        if (button == MOUSE_BUTTON_2) {
            if (action == Action::ACTION_PRESSED) {
                c.camera_input.is_mouse_right_down = true;
            } else if (action == Action::ACTION_RELEASED) {
                c.camera_input.is_mouse_right_down = false;
            }
        }
    });
    c.window->set_cursor_position_callback([this_](float2 xy) {
        auto &c = *static_cast<ContextImpl *>(this_);
        c.camera_input.mouse_cursor_pos = xy;
    });
    c.window->set_key_callback([this_](Key key, KeyModifiers modifiers, Action action) {
        auto &c = *static_cast<ContextImpl *>(this_);
        bool pressed = false;
        if (action == Action::ACTION_PRESSED) {
            pressed = true;
        } else if (action == Action::ACTION_RELEASED) {
            pressed = false;
        } else {
            return;
        }
        switch (key) {
            case Key::KEY_SPACE: {
                c.camera_input.is_space_down = pressed;
            } break;
            case Key::KEY_RIGHT_SHIFT:
            case Key::KEY_LEFT_SHIFT: {
                c.camera_input.is_shift_down = pressed;
            } break;
            case Key::KEY_W: {
                c.camera_input.is_front_dir_key_pressed = pressed;
            } break;
            case Key::KEY_S: {
                c.camera_input.is_back_dir_key_pressed = pressed;
            } break;
            case Key::KEY_A: {
                c.camera_input.is_left_dir_key_pressed = pressed;
            } break;
            case Key::KEY_D: {
                c.camera_input.is_right_dir_key_pressed = pressed;
            } break;
            case Key::KEY_Q: {
                c.camera_input.is_up_dir_key_pressed = pressed;
            } break;
            case Key::KEY_E: {
                c.camera_input.is_down_dir_key_pressed = pressed;
            } break;
        }
    });
}
void RBCContext::disable_camera_control(void *this_) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!c.window) [[unlikely]] {
        LUISA_ERROR("Window instance required for camera control.");
    }
    if (!c.display_cam_entity || !c.display_cam_entity->get_component<world::CameraComponent>()) [[unlikely]] {
        LUISA_ERROR("Display camera instance required for camera control.");
    }
    c.clear_window_event();
    c.cam_controller.reset();
}

void RBCContext::upload_texture_data(void *this_, void *tex) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    auto *tex_res = static_cast<world::TextureResource *>(tex);
    auto *device_img = tex_res->get_image();
    if (!device_img) [[unlikely]] {
        if (tex_res->is_vt()) {
            LUISA_ERROR("Uploading to virtual-texture.");
        } else {
            LUISA_ERROR("Uploading to texture not created or installed.");
        }
    }
    c.utils->update_texture(device_img, ~0u);// Update all mip levels
}

void RBCContext::upload_mesh_data(void *this_, void *mesh) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    auto *mesh_res = static_cast<world::MeshResource *>(mesh);

    auto *device_mesh = mesh_res->device_mesh();
    if (!device_mesh) [[unlikely]] {
        if (mesh_res->is_transforming_mesh()) {
            LUISA_ERROR("Uploading to skinning-mesh.");
        } else {
            LUISA_ERROR("Uploading to mesh not created or installed.");
        }
    }
    c.utils->update_mesh_data(device_mesh, false);// Update all data, not just vertex
}

void RBCContext::update_skinning_mesh(void *this_, void *skinning_mesh, luisa::compute::BufferCreationInfoInterop dual_quaternion_buffer) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    auto *mesh_res = static_cast<world::MeshResource *>(skinning_mesh);
    if (dual_quaternion_buffer.element_stride != sizeof(DualQuaternion)) [[unlikely]] {
        LUISA_ERROR("Skinning buffer stride is not sizeof(DualQuaternion)");
    }
    auto elem_size = dual_quaternion_buffer.total_size_bytes / dual_quaternion_buffer.element_stride;
    c.utils->update_skinning(
        mesh_res,
        luisa::compute::BufferView<DualQuaternion>{
            (void *)dual_quaternion_buffer.native_handle,
            dual_quaternion_buffer.handle,
            dual_quaternion_buffer.element_stride,
            0,
            elem_size, elem_size});
}
void RBCContext::regist_callback(void *this_, luisa::string_view name, luisa::move_only_function<void(rbc::RCBase *)> &&callback) {
    auto &c = *static_cast<ContextImpl *>(this_);

    std::lock_guard lck{c._ctx_mtx};
    rbc::world::regist_callback(
        name,
        reinterpret_cast<luisa::move_only_function<void(void *)> &&>(callback));
}
void RBCContext::unregist_callback(void *this_, luisa::string_view name) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    rbc::world::unregist_callback(name);
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