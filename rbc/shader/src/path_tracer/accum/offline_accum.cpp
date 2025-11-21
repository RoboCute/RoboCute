#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& in, Image<float>& out, Image<float>& temp, uint2 render_resolution, uint frame) {
	auto coord = dispatch_id().xy;
	auto uv = (float2(coord) + 0.5f) / float2(dispatch_size().xy);
	auto sample_id = uint2(float2(render_resolution) * uv);
	auto v = in.read(sample_id);
	v.xyz = spectrum::spectrum_to_tristimulus(v.xyz);
	if (frame > 0) {
		auto old = out.read(coord);
		float alpha = old.w + v.w;
		float3 color = lerp(old.xyz, v.xyz, v.w / max(alpha, 1e-5f));
		v = float4(color, alpha);
	}
	temp.write(coord, v);
	out.write(coord, v);
	return 0;
}