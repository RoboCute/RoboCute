#include <fsd/geometry_accel.hpp>
#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
    BindlessBuffer &heap,
    Buffer<uint> &heads,
    Buffer<uint> &next,
    uint head_offset,
    uint node_offset,
    uint mesh_heap_index,
    uint triangle_byte_offset,
    uint bucket_mask) {
    auto edge_index = dispatch_id().x;
    auto primitive_index = edge_index / 3u;
    auto local_edge_index = edge_index % 3u;
    auto triangle = heap.byte_buffer_read<geometry::Triangle>(
        mesh_heap_index,
        triangle_byte_offset +
            primitive_index * sizeof(geometry::Triangle));
    auto edge = fsd::triangle_edge(triangle, local_edge_index);
    auto bucket = head_offset +
        (fsd::edge_hash(edge.x, edge.y) & bucket_mask);
    auto node = node_offset + edge_index;
    auto previous = heads.atomic_exchange(bucket, node);
    next.write(node, previous);
    return 0;
}
