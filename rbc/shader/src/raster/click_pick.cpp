#include <luisa/std.hpp>
#include <material/mat_codes.hpp>
#include <utils/heap_indices.hpp>

using namespace luisa::shader;

struct RayCastResult {
    float2 triangle_bary;
    uint inst_id;
    uint prim_id;
    uint mat_code;
    uint submesh_index;
};

[[kernel_1d(128)]] int kernel(
    BindlessBuffer &buffer_heap,
    Buffer<float2> &click_buffer,
    Image<uint> &img,
    Buffer<RayCastResult> &out_buffer) {
    auto id = dispatch_id().x;
    uint2 coord = click_buffer.read(id) * float2(img.size());
    auto hit_id = img.read(coord);
    RayCastResult r;
    if (hit_id.x == max_uint32 || hit_id.y == max_uint32) {
        r.inst_id = max_uint32;
        r.prim_id = max_uint32;
    } else {
        r.inst_id = hit_id.x;
        r.prim_id = hit_id.y;
        r.triangle_bary = float2(bit_cast<float>(hit_id.z), bit_cast<float>(hit_id.w));
        auto inst_info = buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, r.inst_id);
        auto mat_meta = material::mat_meta(buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, r.prim_id);
        uint submesh_idx = 0;
        uint mat_index = inst_info.mat_index;
        if (inst_info.mesh.submesh_heap_idx != max_uint32) {
            submesh_idx = buffer_heap.buffer_read<uint16>(inst_info.mesh.submesh_heap_idx, r.prim_id);
            mat_index = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::mat_idx_buffer_heap_idx, mat_index + submesh_idx);
        }
        r.mat_code = mat_index;
        r.submesh_index = submesh_idx;
    }
    out_buffer.write(id, r);
    return 0;
}