#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<float> noisy_buffer, Buffer<float> denoised_buffer, Image<float> img, float weight) {
	auto coord = dispatch_id().xy;
	auto buffer_id = (coord.x + coord.y * dispatch_size().x) * 3;
	float3 noisy_val;
	noisy_val.x = noisy_buffer.read(buffer_id);
	noisy_val.y = noisy_buffer.read(buffer_id + 1);
	noisy_val.z = noisy_buffer.read(buffer_id + 2);
	float3 denoised_val;
	denoised_val.x = denoised_buffer.read(buffer_id);
	denoised_val.y = denoised_buffer.read(buffer_id + 1);
	denoised_val.z = denoised_buffer.read(buffer_id + 2);
	float3 final_val;
	if (weight < 1e-5f) {
		final_val = noisy_val;
	} else if (weight > 0.9999f) {
		final_val = denoised_val;
	} else {
		final_val = lerp(noisy_val, denoised_val, float3(weight));
	};
	img.write(coord, float4(final_val, 1.0f));
	return 0;
}