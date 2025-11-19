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
};

template<typename T>
concept AppDataConcept = requires(T t) {
	t.instance_id;
	t.pos;
};

template<AppDataConcept AppDataType>
InstanceData get_instance_data(
	AppDataType app_data,
	Buffer<geometry::RasterElement>& raster_elem) {
	InstanceData inst_data;
	auto elem = raster_elem.read(object_id() + app_data.instance_id);
	inst_data.local_to_world = elem.local_to_world_and_mat_code;
	inst_data.user_id = bit_cast<uint>(elem.local_to_world_and_mat_code[3][3]);
	inst_data.local_to_world[3][3] = 1.0f;
	return inst_data;
}
inline void transform_projection(float4& proj) {
#ifdef __SHADER_BACKEND_DX
	proj.y *= -1.f;
#endif
}
}// namespace raster