#include <luisa/std.hpp>
#include <luisa/raster/attributes.hpp>
#include <spectrum/color_space.hpp>
#include <sampling/pcg.hpp>
using namespace luisa::shader;
struct AppData {
	[[POSITION]] float2 pos;
	[[UV0]] float2 uv;
	[[COLOR]] float4 col;
};

struct v2p {
	[[POSITION]] float4 pos;
	float4 col;
	float2 uv;
};

struct ObjectData {
	uint tex_id;
	std::array<float, 2> min_view;
	std::array<float, 2> max_view;
};

[[VERTEX_SHADER]] v2p vert(AppData data, float4x4 proj) {
	v2p o;
	o.pos = proj * float4(data.pos, 0.f, 1.f);
	o.col = data.col;
	o.uv = data.uv;
	return o;
}

float3 LinearToST2084(float3 color) {
	float m1 = 2610.0 / 4096.0 / 4;
	float m2 = 2523.0 / 4096.0 * 128;
	float c1 = 3424.0 / 4096.0;
	float c2 = 2413.0 / 4096.0 * 32;
	float c3 = 2392.0 / 4096.0 * 32;
	float3 cp = pow(abs(color), m1);
	return pow((c1 + c2 * cp) / (1.0f + c3 * cp), m2);
}
inline float3 SRGB_encode(float3 v) {
	return select(
		pow(v, (1.0f / 2.4f)) * 1.055f - 0.055f,
		v * 12.92f,
		v <= 0.04045f / 12.92f);
}
inline float3 SRGB_decode(float3 v) {
	return select(
		pow((v + 0.055f) / 1.055f, 2.4f),
		v / 12.92f,
		v <= 0.04045f);
}

[[PIXEL_SHADER]] float4 pixel(
	v2p i,
	BindlessImage arr,
	Buffer<ObjectData> tex_ids,
	Buffer<uint> hdr_buffer,
	Buffer<uint> reverse_rgb_buffer,
	bool hdr,
	bool hdr_10,
	float lum,
	float random_noise_intensity,
	uint frame_index) {
	auto id = tex_ids.read(object_id());
	if (any(i.pos.xy < float2(id.min_view[0], id.min_view[1]) || i.pos.xy > float2(id.max_view[0], id.max_view[1]))) {
		discard();
		return float4(0);
	}
	float4 col = i.col;
	bool is_hdr = false;
	bool reverse_rgb = false;
	if (id.tex_id != max_uint32) {
		uint buffer_idx = id.tex_id / 32;
		uint elem_value = hdr_buffer.read(buffer_idx);
		is_hdr = (elem_value & (1u << (id.tex_id & 31u))) != 0;
		elem_value = reverse_rgb_buffer.read(buffer_idx);
		reverse_rgb = (elem_value & (1u << (id.tex_id & 31u))) != 0;
		col *= arr.uniform_idx_image_sample(id.tex_id, i.uv, Filter::LINEAR_POINT, Address::REPEAT);
	}
	if (!hdr) {
		sampling::PCGSampler sampler(uint3(i.pos.xy, frame_index));
		col = float4(max((sampler.next3f() - 0.5f) * random_noise_intensity + col.xyz, float3(0)), col.w);
	}
	if (hdr && !is_hdr) {
		col.xyz = SRGB_decode(saturate(col.xyz)) * lum;
		if (hdr_10) {
			// col.xyz = spectrum::displayP3_to_rec2020(col.xyz);
			col.xyz *= 80.0f / 10000.0f;
		}
	}
	if (reverse_rgb) {
		col = col.zyxw;
	}
	if (is_hdr) {
		col.w = 1;
	}
	return col;
}