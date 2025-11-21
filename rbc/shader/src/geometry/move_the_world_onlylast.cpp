#include <luisa/std.hpp>
#include <geometry/types.hpp>

using namespace luisa::shader; 

[[kernel_1d(512)]] int kernel(Buffer<float4x4>& last_transform, float3 offset) {
    auto id = dispatch_id().x;
    auto tr = last_transform.read(id);
    tr[3] += float4(offset, 0);
    last_transform.write(id, tr);
    return 0;
}