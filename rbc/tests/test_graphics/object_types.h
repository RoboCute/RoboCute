#pragma once
#include <rbc_graphics/lights.h>
#include <rbc_graphics/light_type.h>
#include <rbc_core/rc.h>
namespace rbc {
struct LightStub : RCBase {
    LightType light_type;
    uint32_t id;
};
enum class ObjectRenderType {
    Mesh,
    EmissionMesh,
    Procedural
};
struct ObjectStub : RCBase {
    RC<DeviceMesh> mesh_ref;
    union {
        uint mesh_tlas_idx;
        uint mesh_light_idx;
        uint procedural_idx;
    };
    ObjectRenderType type;
    luisa::vector<MatCode> material_codes;
};
}// namespace rbc