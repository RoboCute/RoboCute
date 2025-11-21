#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& world_mv_img,
	Image<float>& depth_img,
	Image<float>& out_depth_img,
	Image<float>& screen_mv_img,
	Image<float>& confidence_img,
	Image<float>& reactive_img,
	float4x4 p,
	float4x4 inv_p,
	float4x4 inv_v,
	float4x4 last_vp,
	float4x4 inv_sky_view,// camera view-matrix with no translation
	float4x4 last_sky_vp  // camera view-matrix with no translation
) {

	int2 coord = dispatch_id().xy;
	int2 size = dispatch_size().xy;
	auto view_z = depth_img.read(coord).x;
	auto nonjitter_uv = (float2(coord) + float2(0.5)) / float2(size);
	if (view_z < 1e-10 || view_z > 1e5) {
		view_z = 1e6f;
	}
	auto proj_pos = float4((nonjitter_uv * 2.f - 1.0f), 0.5f, 1);
	auto view_pos = inv_p * proj_pos;
	view_pos /= view_pos.w;
	auto view_dir = normalize(view_pos.xyz);
	float3 dst_view_pos = view_dir * abs(view_z / view_dir.z);
	float4 dst_world_pos = inv_v * float4(dst_view_pos, 1.f);
	dst_world_pos /= dst_world_pos.w;
	if (view_z > 1e5f) {
		float4 sky_world_pos = inv_sky_view * view_pos;
		sky_world_pos /= sky_world_pos.w;
		auto sky_proj = last_sky_vp * float4(sky_world_pos);
		sky_proj /= sky_proj.w;
		float2 last_uv = sky_proj.xy * 0.5f + 0.5f;
		screen_mv_img.write(coord, float4(nonjitter_uv - last_uv, 0.f, 0.f));
	} else {
		auto last_world_pos = dst_world_pos.xyz - world_mv_img.read(coord).xyz;
		auto last_proj = last_vp * float4(last_world_pos, 1.f);
		float2 last_uv = (last_proj.xy / last_proj.w) * 0.5f + 0.5f;
		screen_mv_img.write(coord, float4(nonjitter_uv - last_uv, 0.f, 0.f));
	}

	float4 dst_proj = p * float4(dst_view_pos, 1.f);
	out_depth_img.write(coord, float4(dst_proj.z / dst_proj.w));
	float reactive = 1.0f;
	for (int x = max(0, coord.x - 1); x < min(size.x - 1, coord.x + 1); ++x)
		for (int y = max(0, coord.y - 1); y < min(size.y - 1, coord.y + 1); ++y) {
			reactive = min(reactive, lerp(0.5f, 0.f, confidence_img.read(uint2(x, y)).x));
		}
	reactive_img.write(coord, float4(reactive));

	return 0;
}