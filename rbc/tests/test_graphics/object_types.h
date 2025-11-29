#pragma once
#include <rbc_graphics/lights.h>
#include <rbc_graphics/light_type.h>
#include <rbc_core/rc.h>
namespace rbc {
struct LightStub : RCBase {
    LightType light_type;
    uint32_t id;
};
struct ObjectStub : RCBase {
    RC<DeviceMesh> mesh_ref;
    uint tlas_idx;
};
}// namespace stub_obj