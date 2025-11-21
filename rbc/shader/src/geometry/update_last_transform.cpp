#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(512)]] int kernel(Accel& accel, Buffer<float4x4>& last_transform) {
    auto dsp_id = dispatch_id().x;
    auto user_id = accel.instance_user_id(dsp_id);
    last_transform.write(user_id, accel.instance_transform(dsp_id));
	return 0;
}