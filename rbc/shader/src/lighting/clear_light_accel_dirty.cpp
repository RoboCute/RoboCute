#include <luisa/std.hpp>
#include <lighting/types.hpp>

using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
	Image<uint>& img,
	std::array<uint, lighting::LightTypes::LightCount> arr) {
	auto id = dispatch_id().xy;
	auto size = arr[id.y];
	auto light_id = id * 8u;
	uint result = 0u;
	if (light_id.x < size) {
		if (size - light_id.x < 8u) {
			result = (1u << (size - light_id.x)) - 1u;
			// high bit should be 1
			result = result ^ 255u;
		};
	} else {
		result = 255u;
	};
	img.write(id, uint4(result));
	return 0;
}