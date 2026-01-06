#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;
[[kernel_1d(64)]] int kernel(
	Buffer<uint>& buffer, uint value) {
	buffer.write(dispatch_id().x, value);
	return 0;
}