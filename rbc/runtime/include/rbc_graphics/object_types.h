#pragma once
#include <rbc_graphics/lights.h>
#include <rbc_graphics/light_type.h>
#include <rbc_core/rc.h>
namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
struct RBC_RUNTIME_API LightStub : RCBase {
    LightType light_type{(LightType)-1};
    uint32_t id;
    LightStub() = default;
    ~LightStub();
    void add_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible);
    void add_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible);
    void add_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible);
    void add_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible);
    void update_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible);
    void update_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible);
    void update_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible);
    void update_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible);
};
enum class ObjectRenderType {
    Mesh,
    EmissionMesh,
    Procedural
};
struct RBC_RUNTIME_API MaterialStub : RCBase {
    using MatDataType = vstd::variant<
        material::OpenPBR,
        material::Unlit>;
    MatCode mat_code;
    MatDataType mat_data;
    void craete_pbr_material();
    void update_material(luisa::string_view json);
    void remove_material();
    MaterialStub();
    ~MaterialStub();
    static void openpbr_json_ser(JsonSerializer &json_ser, material::OpenPBR const &mat);
    static void openpbr_json_deser(JsonDeSerializer &json_deser, material::OpenPBR &mat);
    static void openpbr_json_ser(JsonSerializer &json_ser, material::Unlit const &mat);
    static void openpbr_json_deser(JsonDeSerializer &json_deser, material::Unlit &mat);
};
struct RBC_RUNTIME_API ObjectStub : RCBase {
    RC<DeviceMesh> mesh_ref;
    union {
        uint mesh_tlas_idx;
        uint mesh_light_idx;
        uint procedural_idx;
    };
    ObjectRenderType type;
    luisa::vector<RC<MaterialStub>> materials;
    luisa::vector<MatCode> material_codes;
    void create_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> materials);
    void update_object_pos(luisa::float4x4 matrix);
    void update_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> materials);
    uint get_tlas_index() const;
    void remove_object();
    ObjectStub();
    ~ObjectStub();
};

}// namespace rbc