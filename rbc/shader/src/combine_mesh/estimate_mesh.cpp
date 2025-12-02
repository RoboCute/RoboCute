#define DEBUG
#include <luisa/printer.hpp>
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <geometry/vertices.hpp>
#include <sampling/pcg.hpp>
#include <material/mats.hpp>
#include <post_process/colors.hpp>
using namespace luisa::shader;
[[kernel_1d(128)]] int kernel(
    BindlessBuffer &heap,
    ByteBuffer<> &mesh_buffer,
    BindlessImage &image_heap,
    // mesh meta
    uint submesh_heap_idx,
    uint vertex_count,
    uint tri_byte_offset,
    uint ele_mask,

    uint mat_index,
    uint mat_idx_buffer_heap_idx,
    Buffer<float> &result) {
    auto prim_id = dispatch_id().x;
    sampling::PCGSampler pcg(prim_id);
    float3 sum;
    constexpr uint SPP = 16;
    auto uvs = geometry::read_vert_uv(mesh_buffer, prim_id, vertex_count, tri_byte_offset, ele_mask);
    for (uint i = 0; i < SPP; ++i) {
        auto bary = pcg.next2f();
        auto uv = geometry::interpolate(bary, uvs[0], uvs[1], uvs[2]);
        auto emission = material::get_light_emission(heap, image_heap, mat_idx_buffer_heap_idx, submesh_heap_idx, mat_index, prim_id, uv);
        sum += emission;
    }
    sum = sum / float(SPP);
    auto lum = Luminance(sum);
    result.write(prim_id, lum);
    return 0;
}