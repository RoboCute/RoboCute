#include <fsd/geometry_accel.hpp>
#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
    BindlessBuffer &heap,
    Buffer<uint> &heads,
    Buffer<uint> &next,
    Buffer<uint> &adjacency,
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

    uint neighbor = max_uint32;
    auto node = heads.read(bucket);
    while (node != max_uint32) {
        auto candidate_edge_index = node - node_offset;
        auto candidate_primitive = candidate_edge_index / 3u;
        if (candidate_primitive != primitive_index) {
            auto candidate_triangle =
                heap.byte_buffer_read<geometry::Triangle>(
                    mesh_heap_index,
                    triangle_byte_offset +
                        candidate_primitive * sizeof(geometry::Triangle));
            auto candidate_edge = fsd::triangle_edge(
                candidate_triangle,
                candidate_edge_index % 3u);
            if (all(candidate_edge == edge)) {
                neighbor = candidate_primitive;
                break;
            }
        }
        node = next.read(node);
    }
    adjacency.write(node_offset + edge_index, neighbor);
    return 0;
}
