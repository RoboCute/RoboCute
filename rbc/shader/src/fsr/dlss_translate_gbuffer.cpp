#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 16)]] int kernel(
	Image<float>& world_mv_img,
	Image<float>& viewz_img,
	Image<float>& screen_mv_img,
	Image<float>& normal_roughness,
	Image<float>& out_normal_roughness,
	float4x4 inv_p,
	float4x4 inv_v,
	float4x4 last_vp,
	float4x4 inv_sky_view,// camera view-matrix with no translation
	float4x4 last_sky_vp, // camera view-matrix with no translation
	float near_z,
	float far_z) {

	auto coord = dispatch_id().xy;
	auto size = dispatch_size().xy;
	auto view_z = viewz_img.read(coord).x;
	auto nonjitter_uv = (float2(coord) + float2(0.5)) / float2(size);
	auto proj_pos = float4((nonjitter_uv * 2.f - 1.0f), 0.5f, 1);
	auto view_pos = inv_p * proj_pos;
	view_pos /= view_pos.w;
	if (view_z < 0.f || view_z > 1e5f) {
		float4 sky_world_pos = inv_sky_view * view_pos;
		sky_world_pos /= sky_world_pos.w;
		auto sky_proj = last_sky_vp * float4(sky_world_pos);
		sky_proj /= sky_proj.w;
		float2 last_uv = sky_proj.xy * 0.5f + 0.5f;
		screen_mv_img.write(coord, float4(nonjitter_uv - last_uv, 0.f, 0.f));
		// viewz_img.write(coord, float4(1.0f));
	} else {
		auto view_dir = normalize(view_pos.xyz);
		float3 dst_view_pos = view_dir * abs(view_z / view_dir.z);
		float4 dst_world_pos = inv_v * float4(dst_view_pos, 1.f);
		dst_world_pos /= dst_world_pos.w;
		auto last_world_pos = dst_world_pos.xyz - world_mv_img.read(coord).xyz;
		auto last_proj = last_vp * float4(last_world_pos, 1.f);
		float2 last_uv = (last_proj.xy / last_proj.w) * 0.5f + 0.5f;
		screen_mv_img.write(coord, float4(nonjitter_uv - last_uv, 0.f, 0.f));
		float3 norm_rough = normal_roughness.read(coord).xyz;
		float4 norm_rough_result(
			sampling::decode_unit_vector(norm_rough.xy),
			norm_rough.z);
		out_normal_roughness.write(coord, norm_rough_result);
		// view_z = saturate(1.0f - (far_z - view_z) / (far_z - near_z));
		// viewz_img.write(coord, float4(view_z));
	}
	return 0;
}