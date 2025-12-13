#include <rbc_world_v2/light.h>
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_graphics/render_device.h>

namespace rbc::world {
Light::Light(Entity *entity) : ComponentDerive<Light>(entity) {}
Light::~Light() {}
void Light::_update_light() {
    auto tr = entity()->get_component<Transform>();
    if (!tr) return;
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
void Light::on_awake() {
    add_event(WorldEventType::OnTransformUpdate, &Light::_update_light);
}
void Light::on_destroy() {
    _light_stub.remove_light();
}
void Light::serialize(ObjSerialize const&ser) const {
    auto & ser_obj = ser.ser;
    ser_obj._store(_luminance, "luminance");
    ser_obj._store(_angle_radians, "angle_radians");
    ser_obj._store(_small_angle_radians, "small_angle_radians");
    ser_obj._store(_angle_atten_pow, "angle_atten_pow");
    ser_obj._store(_visible, "visible");
    ser_obj._store((uint)_light_stub.light_type, "light_type");
}
void Light::deserialize(ObjDeSerialize const&ser) {
    auto & obj = ser.ser;
    obj._load(_luminance, "luminance");
    obj._load(_angle_radians, "angle_radians");
    obj._load(_small_angle_radians, "small_angle_radians");
    obj._load(_angle_atten_pow, "angle_atten_pow");
    obj._load(_visible, "visible");
    uint light_type;
    if (obj._load(light_type, "light_type")) {
        _light_stub.light_type = (LightType)light_type;
    }
}
void Light::add_area_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Area) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Area;
    _update_light();
}
void Light::add_disk_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Disk) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Disk;
    _update_light();
}
void Light::add_point_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    if (_light_stub.light_type != LightType::Sphere) {
        _light_stub.remove_light();
    }
    _light_stub.light_type = LightType::Sphere;
    _update_light();
}
void Light::add_spot_light(luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
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
DECLARE_WORLD_COMPONENT_REGISTER(Light);
}// namespace rbc::world
