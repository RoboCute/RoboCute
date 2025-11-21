#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
	Buffer<uint>& dst,
	Buffer<uint>& offsets) {
	dst.write(offsets.read(kernel_id()) + dispatch_id().x, kernel_id());
	return 0;
}
