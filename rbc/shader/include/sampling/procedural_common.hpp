#pragma once
#define PROCEDURAL_TRACE_MAX_DIST 1e10f
#define PROCEDURAL_TRACE_CHECK_DIST 1e8f

#ifndef DEFINED_G_ACCEL
#include <luisa/std.hpp>
namespace luisa::shader {
extern Accel &g_accel;
}// namespace luisa::shader
#endif

namespace luisa::shader {
struct ProceduralID {
    uint _id;
    uint mat_idx;
    uint mat_offset;
    uint sh_degree;
    uint type_id() {
        return _id >> 28u;
    }
    uint mat_heap_id() {
        return _id & ((1u << 28u) - 1);
    }
    void set_id(uint type, uint mat_heap_id) {
        _id = (type << 28u) | (mat_heap_id & ((1u << 28u) - 1));
    }
};
struct ProceduralGeometry {
    float3 normal;
    ProceduralID procedural_id;
};
}// namespace luisa::shader