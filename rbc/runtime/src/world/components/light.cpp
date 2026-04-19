#include <rbc_world/components/light_component.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/entity.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>

namespace rbc::world {

namespace {
    void set_light_type(LightStub &stub, LightType type) {
        if (stub.light_type != type) {
            stub.remove_light();
        }
        stub.light_type = type;
    }

    void add_light_by_type(
        LightStub &stub,
        const float4x4 &matrix,
        float half_scale,
        float3 position,
        float3 direction,
        float3 luminance,
        bool visible,
        float angle_radians,
        float small_angle_radians,
        float angle_atten_pow) {
        switch (stub.light_type) {
            case LightType::Area:
                stub.add_area_light(matrix, luminance, visible);
                break;
            case LightType::Disk:
                stub.add_disk_light(position, half_scale, luminance, direction, visible);
                break;
            case LightType::Sphere:
                stub.add_point_light(position, half_scale, luminance, visible);
                break;
            case LightType::Spot:
                stub.add_spot_light(position, half_scale, luminance, direction, angle_radians, small_angle_radians, angle_atten_pow, visible);
                break;
            default:
                break;
        }
    }

    void update_light_by_type(
        LightStub &stub,
        const float4x4 &matrix,
        float scale,
        float3 position,
        float3 direction,
        float3 luminance,
        bool visible,
        float angle_radians,
        float small_angle_radians,
        float angle_atten_pow) {
        switch (stub.light_type) {
            case LightType::Area:
                stub.update_area_light(matrix, luminance, visible);
                break;
            case LightType::Disk:
                stub.update_disk_light(position, scale, luminance, direction, visible);
                break;
            case LightType::Sphere:
                stub.update_point_light(position, scale, luminance, visible);
                break;
            case LightType::Spot:
                stub.update_spot_light(position, scale, luminance, direction, angle_radians, small_angle_radians, angle_atten_pow, visible);
                break;
            default:
                break;
        }
    }
}// namespace

LightComponent::LightComponent() {}
LightComponent::~LightComponent() {}
void LightComponent::update_data() {
    auto tr = entity()->get_component<TransformComponent>();
    if (!tr) return;
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Light::update_data can only be called in render-thread.");
    }
    auto matrix = tr->trs_float();
    auto scale_vec = tr->scale();
    auto scale = max(scale_vec.x, max(scale_vec.y, scale_vec.z));
    auto half_scale = scale * 0.5f;
    auto position = make_float3(tr->position());
    auto direction = normalize(matrix[2].xyz());
    if (_light_stub.id == ~0u) {
        add_light_by_type(
            _light_stub, matrix, half_scale, position, direction,
            _luminance, _visible, _angle_radians, _small_angle_radians, _angle_atten_pow);
    } else {
        update_light_by_type(
            _light_stub, matrix, scale, position, direction,
            _luminance, _visible, _angle_radians, _small_angle_radians, _angle_atten_pow);
    }
}
void LightComponent::on_awake() {
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->add_on_update_event(this, &LightComponent::update_data);
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
    set_light_type(_light_stub, LightType::Area);
    update_data();
}
void LightComponent::add_disk_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    set_light_type(_light_stub, LightType::Disk);
    update_data();
}
void LightComponent::add_point_light(luisa::float3 luminance, bool visible) {
    _luminance = luminance;
    _visible = visible;
    set_light_type(_light_stub, LightType::Sphere);
    update_data();
}
void LightComponent::add_spot_light(luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    _luminance = luminance;
    _visible = visible;
    _angle_atten_pow = angle_atten_pow;
    _angle_radians = angle_radians;
    _small_angle_radians = small_angle_radians;
    set_light_type(_light_stub, LightType::Spot);
    update_data();
}
DECLARE_WORLD_OBJECT_REGISTER(LightComponent);

}// namespace rbc::world
