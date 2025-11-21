#include <surfel/surfel_grid.hpp>
#include <utils/onb.hpp>
#include <sampling/sample_funcs.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<uint>& sample_flags_img,
	Image<float>& confidence_img,
	Image<float>& radiance_img,
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	float grid_element_size,
	Image<uint>& surfel_mark) {
	auto id = dispatch_id().xy;

	auto key_slot = surfel_mark.read(id).x;
	if (key_slot == max_uint32) {
		return 0;
	}

	auto slot = key_slot * SURFEL_INT_SIZE;
	auto confidence = float(value_buffer.read(slot + 8u)) / float(max_uint32);
	confidence_img.write(id, float4(min(confidence_img.read(id).x, confidence)));
	float3 radiance = float3(
		bit_cast<float>(value_buffer.read(slot)),
		bit_cast<float>(value_buffer.read(slot + 1)),
		bit_cast<float>(value_buffer.read(slot + 2)));
	radiance /= min(float(MAX_ACCUM), float(value_buffer.read(slot + 3)));
	float4 last = radiance_img.read(id);
	auto key = key_buffer.read(key_slot);
	auto level = key & 255u;

	float end_len = length(float3(grid_element_size * pow(2.f, float(level))));
	float close_cull = saturate((end_len - last.w) / end_len);
	// near: 1
	// far: 0
	close_cull = 1.f - close_cull;
	radiance = lerp(last.xyz, radiance, close_cull * (1.0f - 1.0f / (float(MAX_ACCUM) * 0.5f)));// TODO: difference with material
	last = float4(radiance, last.w);
	radiance_img.write(id, last);
	return 0;
}