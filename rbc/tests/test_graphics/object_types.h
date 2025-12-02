#pragma once
#include <rbc_graphics/lights.h>
#include <rbc_graphics/light_type.h>
#include <rbc_core/rc.h>
namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
struct LightStub : RCBase {
    LightType light_type;
    uint32_t id;
    LightStub() = default;
    ~LightStub();
};
enum class ObjectRenderType {
    Mesh,
    EmissionMesh,
    Procedural
};
struct MaterialStub : RCBase {
    using MatDataType = vstd::variant<
        material::OpenPBR,
        material::Unlit>;
    MatCode mat_code;
    MatDataType mat_data;
    MaterialStub() = default;
    ~MaterialStub();
};
struct ObjectStub : RCBase {
    RC<DeviceMesh> mesh_ref;
    union {
        uint mesh_tlas_idx;
        uint mesh_light_idx;
        uint procedural_idx;
    };
    ObjectRenderType type;
    luisa::vector<RC<MaterialStub>> materials;
    luisa::vector<MatCode> material_codes;
    ObjectStub() = default;
    ~ObjectStub();
};
}// namespace rbc