#include <luisa/std.hpp>
#include <luisa/types/ray.hpp>

using namespace luisa::shader;

/// Compute AABB for height field blocks using warp operations and shared memory
/// Each block processes a 32x32 region of the height map
[[kernel_2d(32, 32)]] [[warp_size(32)]]
int kernel(
    Image<float> &height_image,
    Buffer<AABB> &output_buffer,
    Buffer<float2> &height_min_max_buffer,
    float2 xz_axis_min,
    float2 xz_axis_max) {

    // Get dispatch IDs
    uint2 block_id_xy = block_id().xy;
    uint2 thread_id_xy = thread_id().xy;
    uint2 dispatch_id_xy = dispatch_id().xy;

    // Get image size
    uint2 img_size = height_image.size();

    // Calculate the number of blocks in x direction
    uint blocks_x = (img_size.x + 31u) / 32u;

    // Calculate block index for output
    uint block_idx = block_id_xy.y * blocks_x + block_id_xy.x;

    // Read height value at this thread's position
    float height_min = 0.0f;
    float height_max = 0.0f;
    if (all(dispatch_id_xy < img_size)) {
        float4 height_val = height_image.read(dispatch_id_xy);
        height_min = height_val.x;
        height_max = height_min;
    } else {
        // For out-of-bounds threads, use neutral values that won't affect min/max
        height_min = 1e8f; // Will be ignored for max comparison
        height_max = -1e8f;// Will be ignored for max comparison
    }

    // Use warp operations to find min/max height within the block
    // First, reduce within each warp (32 threads)
    float warp_min = height_min;
    float warp_max = height_max;

    // Warp reduction using shuffle operations
    // The warp_active_min/max operations perform reduction across all active lanes
    warp_min = warp_active_min(warp_min);
    warp_max = warp_active_max(warp_max);

    // Use SharedArray to store per-warp results for final reduction
    // A 32x32 block has 1024 threads = 32 warps (assuming warp size of 32)

    SharedArray<uint, 1> warp_counter;
    uint lane_id = warp_lane_id();
    // get warp_id
    uint warp_id;
    if (all(thread_id_xy == 0u)) {
        warp_counter[0] = 0u;
    }
    sync_block();
    if (lane_id == 0) {
        warp_id = warp_counter.atomic_fetch_add(0, 1);
    }
    warp_id = warp_read_first_active_lane(warp_id);

    // Each warp leader writes its result to shared memory
    SharedArray<float, 32> shared_mins;
    SharedArray<float, 32> shared_maxs;
    if (lane_id == 0u) {
        shared_mins[warp_id] = warp_min;
        shared_maxs[warp_id] = warp_max;
    }
    // Warp  Use this method to check: is current thread  in the first warp of the whole block?
    uint is_first_thread = all(thread_id_xy == 0u) ? max_uint32 : 0u;
    bool thread_in_first_warp = warp_active_max(is_first_thread) != 0u;

    sync_block();

    // First warp performs final reduction across all warps
    float block_min = 1e30f;
    float block_max = -1e30f;

    if (warp_id == 0u) {
        // Read all warp results and reduce
        block_min = shared_mins[lane_id];
        block_max = shared_maxs[lane_id];

        // Final warp reduction
        block_min = warp_active_min(block_min);
        block_max = warp_active_max(block_max);

        // Write result to output buffer (only first thread)
        if (lane_id == 0u) {
            float2 uv_start = float2(dispatch_id_xy) / float2(img_size);
            float2 uv_end = (1.0f + float2(dispatch_id_xy)) / float2(img_size);
            float2 min_xz = lerp(xz_axis_min, xz_axis_max, uv_start);
            float2 max_xz = lerp(xz_axis_min, xz_axis_max, uv_end);
            // Build AABB
            AABB aabb;
            aabb.packed_min[0] = min_xz.x;
            aabb.packed_min[1] = block_min;
            aabb.packed_min[2] = min_xz.y;

            aabb.packed_max[0] = max_xz.x;
            aabb.packed_max[1] = block_max;
            aabb.packed_max[2] = max_xz.y;

            output_buffer.write(block_idx, aabb);
            height_min_max_buffer.write(block_idx, float2(block_min, block_max));
        }
    }

    return 0;
}
