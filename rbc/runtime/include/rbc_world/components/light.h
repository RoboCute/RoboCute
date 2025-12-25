#pragma once
#include <rbc_world/component.h>
#include <rbc_graphics/object_types.h>

namespace rbc::world {
struct RBC_RUNTIME_API LightComponent final : ComponentDerive<LightComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(LightComponent)
private:
    LightStub _light_stub;
    LightComponent(Entity *entity);
    ~LightComponent();
    float3 _luminance{1, 1, 1};
    float _angle_radians{1.04719755119659f};       // 60
    float _small_angle_radians{0.349065850398865f};// 20
    float _angle_atten_pow{1.0f};
    bool _visible{true};
    void _update_light();
public:
    [[nodiscard]] auto luminance() const { return _luminance; }
    [[nodiscard]] auto angle_radians() const { return _angle_radians; }
    [[nodiscard]] auto small_angle_radians() const { return _small_angle_radians; }
    [[nodiscard]] auto angle_atten_pow() const { return _angle_atten_pow; }
    [[nodiscard]] auto visible() const { return _visible; }
    void add_area_light(luisa::float3 luminance, bool visible);
    void add_disk_light(luisa::float3 luminance, bool visible);
    void add_point_light(luisa::float3 luminance, bool visible);
    void add_spot_light(luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible);
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::LightComponent)