#include <rbc_world/components/light.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/entity.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>

namespace rbc::world {

LightComponent::LightComponent(Entity *entity) : ComponentDerive<LightComponent>(entity) {}
LightComponent::~LightComponent() {}
void LightComponent::_update_light() {
    auto tr = entity()->get_component<TransformComponent>();
    if (!tr) return;
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Light::_update_light can only be called in render-thread.");
    }
    auto matrix = tr->trs_float();
    auto scale_vec = tr->scale();
    auto scale = max(scale_vec.x, max(scale_vec.y, scale_vec.z));
    if (_light_stub.id == ~0u) {
        switch (_light_stub.light_type) {
            case LightType::Area:
                _light_stub.add_area_light(
                    matrix, _luminance, _visible);
                break;
            case LightType::Disk:
                _light_stub.add_disk_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    normalize(matrix[2].xyz()),
                    _visible);
                break;
            case LightType::Sphere:
                _light_stub.add_point_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    _visible);
                break;
            case LightType::Spot:
                _light_stub.add_spot_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    normalize(matrix[2].xyz()),
                    _angle_radians,
                    _small_angle_radians,
                    _angle_atten_pow,
                    _visible);
                break;
            default:
                break;
        }
    } else {
        switch (_light_stub.light_type) {
            case LightType::Area:
                _light_stub.update_area_light(matrix, _luminance, _visible);
                break;
            case LightType::Disk:
                _light_stub.update_disk_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    normalize(matrix[2].xyz()),
                    _visible);
                break;
            case LightType::Sphere:
                _light_stub.update_point_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    _visible);
                break;
            case LightType::Spot:
                _light_stub.update_spot_light(
                    make_float3(tr->position()),
                    scale,
                    _luminance,
                    normalize(matrix[2].xyz()),
                    _angle_radians,
                    _small_angle_radians,
                    _angle_atten_pow,
                    _visible);
                break;
            default:
                break;
        }
    }
}
void LightComponent::on_awake() {
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->add_on_update_event(this, &LightComponent::_update_light);
    }
}
void LightComponent::on_destroy() {
    _light_stub.remove_light();
}
void LightComponent::serialize_meta(ObjSerialize const &ser) const {
    ser.ar.value(_luminance, "luminance");
    ser.ar.value(_angle_radians, "angle_radians");
    ser.ar.value(_small_angle_radians, "small_angle_radians");
    ser.ar.value(_angle_atten_pow, "angle_atten_pow");
    ser.ar.value(_visible, "visible");
    ser.ar.value((uint)_light_stub.light_type, "light_type");
}
void LightComponent::deserialize_meta(ObjDeSerialize const &ser) {
    ser.ar.value(_luminance, "luminance");
    ser.ar.value(_angle_radians, "angle_radians");
    ser.ar.value(_small_angle_radians, "small_angle_radians");
    ser.ar.value(_angle_atten_pow, "angle_atten_pow");
    ser.ar.value(_visible, "visible");
    uint light_type;
    if (ser.ar.value(light_type, "light_type")) {
        _light_stub.light_type = (LightType)light_type;
    }
}
void LightComponent::add_area_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Area) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Area;
    _update_light();
}
void LightComponent::add_disk_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Disk) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Disk;
    _update_light();
}
void LightComponent::add_point_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Sphere) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Sphere;
    _update_light();
}
void LightComponent::add_spot_light(luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    _luminance = luminance;
    _visible = visible;
    _angle_atten_pow = angle_atten_pow;
    _angle_radians = angle_radians;
    _small_angle_radians = small_angle_radians;
    if (_light_stub.light_type != LightType::Spot) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Spot;
    _update_light();
}
DECLARE_WORLD_COMPONENT_REGISTER(LightComponent);

}// namespace rbc::world
