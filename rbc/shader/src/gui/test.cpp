#include <luisa/std.hpp>
#include <luisa/raster/attributes.hpp>
using namespace luisa::shader;
struct AppData {
	[[POSITION]] float3 pos;
};

struct v2p {
	[[POSITION]] float4 pos;
	float value;
};

struct ObjectData {
	uint tex_id;
	std::array<float, 2> min_view;
	std::array<float, 2> max_view;
};

[[VERTEX_SHADER]] v2p vert(AppData data, float proj) {
	v2p o;
	o.pos = float4(data.pos, 1);
	o.value = data.pos.z;
	return o;
}
[[PIXEL_SHADER]] float4 pixel(
	v2p i,
    float t
) {
	
    return float4(float3(i.value), 1);
}