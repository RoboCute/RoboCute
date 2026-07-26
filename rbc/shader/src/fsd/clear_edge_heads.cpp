#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(Buffer<uint> &heads) {
    heads.write(dispatch_id().x, max_uint32);
    return 0;
}
