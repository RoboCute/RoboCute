#pragma once
#include <luisa/std.hpp>
#include <geometry/types.hpp>
#include <luisa/raster/attributes.hpp>
#include <utils/onb.hpp>

namespace raster {
using namespace luisa::shader;
struct AppDataBase {
    [[POSITION]] float4 pos;
    [[INSTANCE_ID]] uint instance_id;
};
struct InstanceData {
    float4x4 local_to_world;
    uint user_id;
    uint prim_id_offset;
};

template<typename T>
concept AppDataConcept = requires(T t) {
    t.instance_id;
    t.pos;
};

template<AppDataConcept AppDataType>
InstanceData get_instance_data(
    AppDataType app_data,
    geometry::RasterElement elem) {
    InstanceData inst_data;
    inst_data.local_to_world = elem.local_to_world_and_inst_id;
    inst_data.prim_id_offset = bit_cast<uint>(elem.local_to_world_and_inst_id[2][3]);
    inst_data.user_id = bit_cast<uint>(elem.local_to_world_and_inst_id[3][3]);
    inst_data.local_to_world[2][3] = 0.0f;
    inst_data.local_to_world[3][3] = 1.0f;
    return inst_data;
}
template<AppDataConcept AppDataType>
InstanceData get_instance_data(
    AppDataType app_data,
    Buffer<geometry::RasterElement> &raster_elem) {
    auto elem = raster_elem.read(object_id() + app_data.instance_id);
    return get_instance_data(app_data, elem);
}
inline void transform_projection(float4 &proj) {
#ifdef __SHADER_BACKEND_DX
    proj.y *= -1.f;
#endif
}
}// namespace raster