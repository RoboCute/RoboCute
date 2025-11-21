#include <luisa/std.hpp>
#include <luisa/raster/attributes.hpp>
#include <spectrum/color_space.hpp>
#include <sampling/pcg.hpp>
using namespace luisa::shader;

struct AppData {
	[[POSITION]] float4 pos;
	[[NORMAL]] float4 normal;
};

struct v2p {
	[[POSITION]] float4 pos;
	float4 col;
};

[[VERTEX_SHADER]] v2p vert(AppData data, float4x4 proj) {
	v2p o;
	o.pos = proj * float4(data.pos.xyz, 1.f);
#ifdef __SHADER_BACKEND_DX
	o.pos.y *= -1.f;
#endif
	o.col = float4(data.normal.xyz, 1.f);
	return o;
}
[[PIXEL_SHADER]] float4 pixel(v2p i) {
	return normalize(i.col + 0.3f);
}