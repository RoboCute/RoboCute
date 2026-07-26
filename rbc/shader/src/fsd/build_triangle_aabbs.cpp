#include <geometry/types.hpp>
#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
    BindlessBuffer &heap,
    Buffer<AABB> &output_aabbs,
    uint heap_idx,
    uint tri_byte_offset,
    float3 local_expand) {
    auto triangle_idx = dispatch_id().x;
    auto triangle = heap.byte_buffer_read<geometry::Triangle>(
        heap_idx,
        tri_byte_offset + sizeof(geometry::Triangle) * triangle_idx);
    auto position_0 = heap.buffer_read<float3>(heap_idx, triangle[0]);
    auto position_1 = heap.buffer_read<float3>(heap_idx, triangle[1]);
    auto position_2 = heap.buffer_read<float3>(heap_idx, triangle[2]);
    auto expansion = max(local_expand, float3(0.0f));
    auto bound_min = min(position_0, min(position_1, position_2)) - expansion;
    auto bound_max = max(position_0, max(position_1, position_2)) + expansion;

    AABB bound;
    for (uint i = 0u; i < 3u; ++i) {
        bound.packed_min[i] = bound_min[i];
        bound.packed_max[i] = bound_max[i];
    }
    output_aabbs.write(triangle_idx, bound);
    return 0;
}
