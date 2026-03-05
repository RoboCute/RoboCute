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
#include <rbc_world/resources/material.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/camera_component.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
#include <rbc_render/renderer_data.h>
#include <rbc_app/camera_controller.h>
#include <luisa/runtime/buffer.h>
#include <rbc_world/callback_serializer.h>
#include "builtin_shader.h"
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
    void reset_view(uint2 resolution) {
        if (window)
            utils->resize_swapchain(resolution, window->native_display(), window->native_handle());
        else
            utils->resize_swapchain(resolution, invalid_resource_handle, invalid_resource_handle);
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
    if (!RenderDevice::instance_ptr()) [[unlikely]] {
        LUISA_ERROR("init_device required before init_render.");
    }
    c.utils->init_render();
}
void RBCContext::init_display(void *this_, luisa::string_view name, uint2 size, bool create_window, bool window_resizable) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!RenderDevice::instance_ptr()) [[unlikely]] {
        LUISA_ERROR("init_device required before init_display.");
    }
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
    c.reset_view(resolution);
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
    if (!img) [[unlikely]] {
        LUISA_ERROR("display not initialized.");
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
            c.reset_view(c.window_size);
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

void RBCContext::control_camera_add_pos(void *this_, luisa::float3 pos) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!c.cam_controller) [[unlikely]] {
        LUISA_ERROR("Camera control not enabled.");
    }
    if (!c.cam_controller->camera) [[unlikely]] {
        LUISA_ERROR("Camera not initialized.");
    }
    if (c.cam_controller->transform)
        c.cam_controller->transform->set_pos(
            c.cam_controller->transform->position() + make_double3(pos.x, pos.y, pos.z),
            false);
    else
        c.cam_controller->camera->position += make_double3(pos.x, pos.y, pos.z);
}

void RBCContext::control_camera_add_rotate(void *this_, float yaw, float pitch, float roll) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!c.cam_controller) [[unlikely]] {
        LUISA_ERROR("Camera control not enabled.");
    }
    c.cam_controller->rotation_yaw += yaw;
    pitch = clamp(pitch, -pi * 0.48f, pi * 0.48f);
    c.cam_controller->rotation_pitch += pitch;
    c.cam_controller->rotation_pitch = clamp(c.cam_controller->rotation_pitch, -pi * 0.48, pi * 0.48);

    c.cam_controller->rotation_roll += roll;
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
    if (_ctx_inst) [[unlikely]] {
        LUISA_ERROR("Context must be singleton.");
    }
    rbc::RuntimeStaticBase::init_all();
    rbc::PluginManager::init();
    auto ptr = new ContextImpl{};
    manually_add_ref(ptr);
    return ptr;
}
void *BuiltinKernels::_create_() {
    auto ptr = new BuiltinShaders();
    manually_add_ref(ptr);
    return ptr;
}
// BuiltinKernels implementations
static bool is_texture_float_type(PixelFormat format) {
    switch (format) {
        // Float-type formats
        case PixelFormat::R16F:
        case PixelFormat::RG16F:
        case PixelFormat::RGBA16F:
        case PixelFormat::R32F:
        case PixelFormat::RG32F:
        case PixelFormat::RGBA32F:
        case PixelFormat::BC6HUF16:
        case PixelFormat::R8UNorm:
        case PixelFormat::RG8UNorm:
        case PixelFormat::RGBA8UNorm:
        case PixelFormat::R16UNorm:
        case PixelFormat::RG16UNorm:
        case PixelFormat::RGBA16UNorm:
        case PixelFormat::BC4UNorm:
        case PixelFormat::BC5UNorm:
        case PixelFormat::BC7UNorm:
        case PixelFormat::BC1UNorm:
        case PixelFormat::BC2UNorm:
        case PixelFormat::BC3UNorm:
        case PixelFormat::R10G10B10A2UNorm:
        case PixelFormat::R11G11B10F:
        case PixelFormat::BC7SRGB:
        case PixelFormat::RGBA8SRGB:
            return true;
        default:
            return false;
    }
}

void BuiltinKernels::buffer_to_image(void *this_, luisa::compute::BufferCreationInfoInterop input_buffer, luisa::compute::TextureCreationInfo output_image, luisa::uint2 pixel_offset, luisa::uint2 pixel_size, luisa::uint4 swizzle) {
    auto &shaders = *static_cast<BuiltinShaders *>(this_);
    auto storage = luisa::compute::pixel_format_to_storage(output_image.format);
    auto &cmdlist = rbc::RenderDevice::instance().lc_main_cmd_list();

    uint swizzle_bytes = BuiltinShaders::compact_swizzle(swizzle);

    // Determine image type based on format
    // Create Buffer<half> or Buffer<float> based on element stride
    auto create_buffer = [&]<typename T>(auto elem) {
        return luisa::compute::BufferView<T>{
            elem.native_handle,
            elem.handle,
            sizeof(T), 0,
            elem.total_size_bytes / sizeof(T),
            elem.total_size_bytes / sizeof(T)};
    };
    if (is_texture_float_type(output_image.format)) {
        // Float image - create from external handle
        luisa::compute::ImageView<float> output_img(
            output_image.native_handle,
            output_image.handle,
            storage,
            output_image.mipmap_levels,
            luisa::uint2(output_image.width, output_image.height));

        if (input_buffer.element_stride == sizeof(luisa::half)) {
            auto input_buf = create_buffer.operator()<half>(input_buffer);
            shaders.dispatch_buffer_to_image<half>(input_buf, output_img, pixel_offset, pixel_size, swizzle);
        } else if (input_buffer.element_stride == sizeof(float)) {
            auto input_buf = create_buffer.operator()<float>(input_buffer);
            shaders.dispatch_buffer_to_image<float>(input_buf, output_img, pixel_offset, pixel_size, swizzle);
        } else {
            LUISA_ERROR("Buffer stride must be 2 bytes (float-16) or 4 bytes (float-32).");
        }
    } else {
        // Int image - treat as uint for shader
        luisa::compute::ImageView<uint> output_img(
            output_image.native_handle,
            output_image.handle,
            storage,
            output_image.mipmap_levels,
            luisa::uint2(output_image.width, output_image.height));

        if (input_buffer.element_stride == sizeof(uint16_t)) {
            auto input_buf = create_buffer.operator()<uint16_t>(input_buffer);
            shaders.dispatch_buffer_to_image<uint16_t>(input_buf, output_img, pixel_offset, pixel_size, swizzle);
        } else if (input_buffer.element_stride == sizeof(uint32_t)) {
            auto input_buf = create_buffer.operator()<uint32_t>(input_buffer);
            shaders.dispatch_buffer_to_image<uint32_t>(input_buf, output_img, pixel_offset, pixel_size, swizzle);
        } else {
            LUISA_ERROR("Buffer stride must be 2 bytes (int-16) or 4 bytes (int-32).");
        }
    }
}

void BuiltinKernels::image_to_buffer(void *this_, luisa::compute::TextureCreationInfo input_image, luisa::compute::BufferCreationInfoInterop output_buffer, luisa::uint2 pixel_offset, luisa::uint2 pixel_size, luisa::uint4 swizzle) {
    auto &shaders = *static_cast<BuiltinShaders *>(this_);
    auto &cmdlist = rbc::RenderDevice::instance().lc_main_cmd_list();

    uint swizzle_bytes = BuiltinShaders::compact_swizzle(swizzle);

    // Determine image type based on format
    // Create Buffer<half> or Buffer<float> based on element stride
    auto create_buffer = [&]<typename T>(auto elem) {
        return luisa::compute::BufferView<T>{
            elem.native_handle,
            elem.handle,
            sizeof(T), 0,
            elem.total_size_bytes / sizeof(T),
            elem.total_size_bytes / sizeof(T)};
    };
    if (is_texture_float_type(input_image.format)) {
        // Float image - create from external handle
        luisa::compute::ImageView<float> input_img(
            input_image.native_handle,
            input_image.handle,
            pixel_format_to_storage(input_image.format),
            input_image.mipmap_levels,
            luisa::uint2(input_image.width, input_image.height));

        if (output_buffer.element_stride == sizeof(luisa::half)) {
            auto output_buf = create_buffer.operator()<half>(output_buffer);
            shaders.dispatch_image_to_buffer<half>(input_img, output_buf, pixel_offset, pixel_size, swizzle);
        } else if (output_buffer.element_stride == sizeof(float)) {
            auto output_buf = create_buffer.operator()<float>(output_buffer);
            shaders.dispatch_image_to_buffer<float>(input_img, output_buf, pixel_offset, pixel_size, swizzle);
        } else {
            LUISA_ERROR("Buffer stride must be 2 bytes (float-16) or 4 bytes (float-32).");
        }
    } else {
        // Int image - treat as uint for shader
        luisa::compute::ImageView<uint> input_img(
            input_image.native_handle,
            input_image.handle,
            pixel_format_to_storage(input_image.format),
            input_image.mipmap_levels,
            luisa::uint2(input_image.width, input_image.height));
        if (output_buffer.element_stride == sizeof(uint16_t)) {
            auto output_buf = create_buffer.operator()<uint16_t>(output_buffer);
            shaders.dispatch_image_to_buffer<uint16_t>(input_img, output_buf, pixel_offset, pixel_size, swizzle);
        } else if (output_buffer.element_stride == sizeof(uint32_t)) {
            auto output_buf = create_buffer.operator()<uint32_t>(output_buffer);
            shaders.dispatch_image_to_buffer<uint32_t>(input_img, output_buf, pixel_offset, pixel_size, swizzle);
        } else {
            LUISA_ERROR("Buffer stride must be 2 bytes (int-16) or 4 bytes (int-32).");
        }
    }
}
void RBCContext::editing_add_click_requires(void *this_, luisa::string_view name, luisa::float2 uv) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!c.display_cam_entity) [[unlikely]] {
        LUISA_ERROR("Display camera not initialized.");
    }
    auto *cam_comp = c.display_cam_entity->get_component<world::CameraComponent>();
    if (!cam_comp) [[unlikely]] {
        LUISA_ERROR("Display camera component not found.");
    }
    auto &render_settings = c.utils->render_settings(static_cast<RenderPlugin::PipeCtxStub *>(cam_comp->render_pipe_ctx()));
    auto *click_mng = &render_settings.read_mut<ClickManager>();
    click_mng->add_require(luisa::string{name}, ClickRequire{.screen_uv = uv});
}
struct SelectQueryImpl : RCBase {
    RC<world::RenderComponent> _comp;
    RC<world::MaterialResource> _mat;
    float2 bary;
    uint submesh_idx{~0u};
    uint prim_id{~0u};
    bool _valid{};
};
void *SelectQuery::_create_() {
    auto p = new SelectQueryImpl{};
    manually_add_ref(p);
    return p;
}

bool SelectQuery::valid(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    return p->_valid;
}
void *SelectQuery::get_component(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    return p->_comp.get();
}
void *SelectQuery::get_material(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    auto mat = p->_mat.get();
    if (!mat) return nullptr;
    manually_add_ref(mat);
    return mat;
}
luisa::float2 SelectQuery::barycentric(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    return p->bary;
}
uint32_t SelectQuery::get_submesh_index(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    return p->submesh_idx;
}
uint32_t SelectQuery::prim_id(void *this_) {
    auto p = static_cast<SelectQueryImpl *>(this_);
    return p->prim_id;
}
void *RBCContext::editing_query_click_requires(void *this_, luisa::string_view name) {
    auto &c = *static_cast<ContextImpl *>(this_);
    std::lock_guard lck{c._ctx_mtx};
    if (!c.display_cam_entity) [[unlikely]] {
        LUISA_ERROR("Display camera not initialized.");
    }
    auto *cam_comp = c.display_cam_entity->get_component<world::CameraComponent>();
    if (!cam_comp) [[unlikely]] {
        LUISA_ERROR("Display camera component not found.");
    }
    auto &render_settings = c.utils->render_settings(static_cast<RenderPlugin::PipeCtxStub *>(cam_comp->render_pipe_ctx()));
    auto *click_mng = &render_settings.read_mut<ClickManager>();
    auto click_result = click_mng->query_result(name);
    auto r = static_cast<SelectQueryImpl *>(SelectQuery::_create_());
    if (!click_result) {
        return r;
    }
    r->_valid = true;
    if (click_result->inst_id == ~0u) {
        return r;
    }
    r->bary = click_result->triangle_bary;
    r->submesh_idx = click_result->submesh_index;
    r->prim_id = click_result->prim_id;
    r->_mat = world::MaterialResource::try_get_resource(MatCode{click_result->mat_code});
    auto elem = SceneManager::instance().accel_manager().try_get_accel_element(click_result->inst_id);
    if (!elem) {
        return r;
    }
    auto ptr = world::RenderComponent::try_get_component(elem->user_id);
    r->_comp = std::move(ptr);
    return r;
}
}// namespace rbc