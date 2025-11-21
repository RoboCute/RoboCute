#include <luisa/std.hpp>

using namespace luisa::shader; 

[[kernel_1d(512)]] int kernel(Accel& accel, Buffer<float4x4>& last_transform, float3 offset) {
    auto id = dispatch_id().x;
    auto tr = accel.instance_transform(id);
    tr[3] += float4(offset, 0);
    accel.set_instance_transform(id, tr);
    id = accel.instance_user_id(id);
    tr = last_transform.read(id);
    tr[3] += float4(offset, 0);
    last_transform.write(id, tr);
    return 0;
}