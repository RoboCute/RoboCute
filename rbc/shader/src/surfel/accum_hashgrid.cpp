#include <surfel/surfel_grid.hpp>
#include <utils/onb.hpp>
#include <sampling/pcg.hpp>
#include <sampling/sample_funcs.hpp>
#include <path_tracer/modulate_result_common.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& radiance_img,
	Buffer<uint>& value_buffer,
	Image<uint>& surfel_mark,
	Buffer<float>& exposure_buffer) {
	auto id = dispatch_id().xy;
	uint key_slot = surfel_mark.read(id).x;
	if (key_slot == max_uint32) {
		return 0;
	}
	auto slot = key_slot * SURFEL_INT_SIZE;
	auto old_count = value_buffer.atomic_fetch_add(slot + 3u, 1);
	if (old_count < MAX_ACCUM) {
		auto radiance_r = radiance_img.read(id);
		radiance_r.xyz = realtime_clamp_radiance(radiance_r.xyz, exposure_buffer);
		old_count = old_count % 3;
		for (uint i = old_count; i < old_count + 3; ++i) {
			auto local_id = i;
			if (local_id >= 3) {
				local_id -= 3;
			}
			float_atomic_add(value_buffer, slot + local_id, radiance_r[local_id]);
		}
	}
	return 0;
}
