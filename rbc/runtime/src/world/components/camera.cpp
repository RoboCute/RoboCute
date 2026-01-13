#include <rbc_world/components/camera_component.h>
#include <rbc_world/entity.h>
#include <rbc_graphics/camera.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_core/state_map.h>

namespace rbc::world {
CameraComponent::CameraComponent() {}
CameraComponent::~CameraComponent() {}
void CameraComponent::serialize_meta(ObjSerialize const &obj) const {
    obj.ar.value(fov, "fov");
    if (auto_aspect_ratio)
        obj.ar.value(auto_aspect_ratio, "auto_aspect_ratio");
    else
        obj.ar.value(aspect_ratio, "aspect_ratio");
    obj.ar.value(near_plane, "near_plane");
    obj.ar.value(far_plane, "far_plane");
    obj.ar.value(enable_physical_camera, "enable_physical_camera");
    obj.ar.value(aperture, "aperture");
    obj.ar.value(focus_distance, "focus_distance");
}
void CameraComponent::deserialize_meta(ObjDeSerialize const &obj) {
    obj.ar.value(fov, "fov");
    if (!obj.ar.value(auto_aspect_ratio, "auto_aspect_ratio") || !auto_aspect_ratio) {
        obj.ar.value(aspect_ratio, "aspect_ratio");
    }
    obj.ar.value(near_plane, "near_plane");
    obj.ar.value(far_plane, "far_plane");
    obj.ar.value(enable_physical_camera, "enable_physical_camera");
    obj.ar.value(aperture, "aperture");
    obj.ar.value(focus_distance, "focus_distance");
}
void CameraComponent::on_awake() {
}
void CameraComponent::_on_transform_update() {
    if (!_render_pipe_ctx) return;
    auto tr = entity()->get_component<TransformComponent>();
    auto &render_settings = GraphicsUtils::instance()->render_settings(static_cast<RenderPlugin::PipeCtxStub *>(_render_pipe_ctx));
    auto &cam = render_settings.read_mut<Camera>();
    cam.rotation_quaternion = tr->rotation();
    cam.position = tr->position();
    fov = clamp(fov, (double)radians(0.001f), (double)radians(179.f));
    cam.fov = fov;
    cam.auto_aspect_ratio = auto_aspect_ratio;
    if (!auto_aspect_ratio) {
        cam.aspect_ratio = aspect_ratio;
    }
    cam.near_plane = near_plane;
    cam.far_plane = far_plane;
    cam.enable_physical_camera = enable_physical_camera;
    if (enable_physical_camera) {
        cam.aperture = aperture;
        cam.focus_distance = focus_distance;
    }
}

void CameraComponent::enable_camera() {
    if (_render_pipe_ctx || !GraphicsUtils::instance()) return;
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->add_on_update_event(this, &CameraComponent::_on_transform_update);
    }
    RenderView rv{
        dst_image ? &dst_image : nullptr,
        view_offset_pixels,
        view_size_pixels};
    _render_pipe_ctx = GraphicsUtils::instance()->register_render_pipectx(rv);
    _on_transform_update();
}
void CameraComponent::disable_camera() {
    if (!_render_pipe_ctx || !GraphicsUtils::instance()) return;
    GraphicsUtils::instance()->remove_render_pipectx(static_cast<RenderPlugin::PipeCtxStub *>(_render_pipe_ctx));
    _render_pipe_ctx = nullptr;
}
void CameraComponent::on_destroy() {
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->remove_on_update_event(this);
    }
    disable_camera();
}
DECLARE_WORLD_OBJECT_REGISTER(CameraComponent)
}// namespace rbc::world