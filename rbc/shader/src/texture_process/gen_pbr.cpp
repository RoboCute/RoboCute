#include <luisa/std.hpp>
#include <spectrum/color_space.hpp>
// #include <sampling/pcg.hpp>
using namespace luisa::shader;
trait_struct ProcessType {
	static constexpr uint32 Normal = 0;
	static constexpr uint32 Rougness = 1;
};
float sobel_x(float3x3 data) {
	return data[0][0] * -1.0f +
		   data[0][1] * -2.0f +
		   data[0][2] * -1.0f +
		   data[2][0] * 1.0f +
		   data[2][1] * 2.0f +
		   data[2][2] * 1.0f;
}
float sobel_y(float3x3 data) {
	return data[0][0] * 1.0f +
		   data[1][0] * 2.0f +
		   data[2][0] * 1.0f +
		   data[0][2] * -1.0f +
		   data[1][2] * -2.0f +
		   data[2][2] * -1.0f;
}

float3 get_normal(SampleImage& albedo_input) {
	auto id = int2(dispatch_id().xy);
	auto size = int2(dispatch_size().xy);
	float3x3 r;
	for (int x = -1; x <= 1; ++x)
		for (int y = -1; y <= 1; ++y) {
			if (all(int2(x, y) == 0)) continue;
			auto sample_id = id + int2(x, y);
			if (any(sample_id < 0 || sample_id >= size)) {
				sample_id = id;
			}
			r[x + 1][y + 1] = reduce_sum(spectrum::srgb_to_acescg(albedo_input.read(sample_id).xyz));
		}
	float2 xy(sobel_x(r), sobel_y(r));
	xy *= 0.5f;
	auto rate = max(1.0f, max(abs(xy.x), abs(xy.y)));
	auto normal = float3(xy, 1) / rate;

	return float3(normal.xy * 0.5f + 0.5f, normal.z);
}

float4 get_rough(SampleImage& albedo_input) {
	auto id = int2(dispatch_id().xy);
	auto size = int2(dispatch_size().xy);
	float2 uv = (float2(id) + 0.5f) / float2(size);
	auto col = albedo_input.sample(uv, Filter::LINEAR_POINT, Address::EDGE);
	float rough = 1;
	for (int i = 0; i < 64; ++i) {
	}
	col.y = rough;
	return col;
}

[[kernel_2d(16, 16)]] int kernel(
	SampleImage& albedo_input,
	Image<float>& output,
	uint type) {
	float4 result;
	switch (type) {
		case ProcessType::Normal: {
			result = float4(get_normal(albedo_input), 1.0f);
		} break;
		case ProcessType::Rougness: {
			result = get_rough(albedo_input);
		} break;
	}
	output.write(dispatch_id().xy, result);
	return 0;
}
