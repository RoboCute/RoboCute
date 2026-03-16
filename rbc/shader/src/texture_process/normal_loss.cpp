#include <luisa/std.hpp>

using namespace luisa::shader;

// Decode normal from [0,1] range (texture storage) to [-1,1] range (direction)
inline float3 decode_normal(float3 encoded_normal) {
    return encoded_normal * 2.0f - 1.0f;
}

[[kernel_1d(1024)]] [[warp_size(32)]]
int kernel(
    Image<float> &src_img,
    Image<float> &dst_img,
    Buffer<float> &result_buffer,
    uint2 size) {
    auto thd_id = thread_id().x;
    float diff = 0;
    auto disp_id = dispatch_id().x;
    uint2 coord(disp_id % size.x, disp_id / size.x);
    if (disp_id < size.x * size.y) {
        // Sample normal from src_img and dst_img, decode from [0,1] to [-1,1], then normalize
        float3 src_normal = normalize(decode_normal(src_img.read(coord).xyz));
        float3 dst_normal = normalize(decode_normal(dst_img.read(coord).xyz));
        // Compute difference: 1 - dot(n1, n2) gives angular difference
        // or we can just use (1 - dot) / 2 to get [0, 1] range
        diff = 1.0f - saturate(dot(src_normal, dst_normal));
    }

    // Use warp and SharedArray, reduce-sum all diff
    // First reduce within warp
    diff = warp_active_sum(diff) / 32.f;

    // Use SharedArray for inter-warp reduction
    // 32x32 threads = 1024 threads, with warp size 32, we have 32 warps
    SharedArray<float, 32> warp_sums;

    auto lane_id = warp_lane_id();
    auto warp_id = thd_id / 32;

    if (wave_is_first_lane()) {
        warp_sums[warp_id] = diff;
    }
    sync_block();

    // First warp reduces all warp sums
    if (warp_id == 0) {
        diff = warp_sums[lane_id];
        diff = warp_active_sum(diff) / 32.f;
    }

    if (thd_id == 0) {
        result_buffer.write(block_id().x, diff);
    }
    return 0;
}
