#include <luisa/std.hpp>

using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
	Image<float>& spec,
	Image<float>& diff,
	Image<float>& last_spec,
	Image<float>& last_diff,
	float rate) {
	auto c = dispatch_id().xy;
	float4 s, d;
	if (rate > 1e-4f) {
		s = lerp(spec.read(c), last_spec.read(c), rate);
		d = lerp(diff.read(c), last_diff.read(c), rate);
	} else {
		s = spec.read(c);
		d = diff.read(c);
	}
	spec.write(c, s);
	diff.write(c, d);
	last_spec.write(c, s);
	last_diff.write(c, d);
	return 0;
}