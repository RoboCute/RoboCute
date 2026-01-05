#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;
[[kernel_1d(128)]] int kernel(
	Buffer<uint16>& buffer, uint value) {
	buffer.write(dispatch_id().x, (uint16)value);
	return 0;
}