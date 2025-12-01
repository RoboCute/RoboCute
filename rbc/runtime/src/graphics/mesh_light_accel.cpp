#include <rbc_graphics/light_type.h>
#include <rbc_graphics/mesh_light_accel.h>
#include <luisa/runtime/device.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
namespace rbc {
auto MeshLightAccel::build_bvh(
    float4x4 matrix,
    span<float3 const> vertices,
    span<Triangle const> triangles,
    span<float const> triangle_lum) -> HostResult {
    HostResult r;

    r.nodes.clear();
    vector<BVH::InputNode> input_nodes;
    auto func = [&](size_t i) {
        BVH::InputNode r;
        auto &&tri = triangles[i];
        float3 p0 = vertices[tri.i0];
        float3 p1 = vertices[tri.i1];
        float3 p2 = vertices[tri.i2];
        p0 = (matrix * make_float4(p0, 1.f)).xyz();
        p1 = (matrix * make_float4(p1, 1.f)).xyz();
        p2 = (matrix * make_float4(p2, 1.f)).xyz();

        r.bounding.min = min(p0, min(p1, p2));
        r.bounding.max = max(p0, max(p1, p2));

        float a = distance(p0, p1);
        float b = distance(p2, p1);
        float c = distance(p0, p2);
        float p = (a + b + c) * 0.5f;
        float area = std::sqrt(p * (p - a) * (p - b) * (p - c));
        r.bary_center_and_weight = make_float4((p0 + p1 + p2) / 3.f, area * triangle_lum[i]);
        auto plane_n = normalize(cross(p1 - p0, p2 - p0));
        r.cone = make_float4(0, 0, 1, pi);
        r.write_index(luisa::to_underlying(LightType::Triangle), i);
        return r;
    };
    input_nodes.push_back_uninitialized(triangles.size());
    std::atomic_size_t input_nodes_count{};
    luisa::fiber::parallel(
        triangles.size(),
        [&](size_t i) {
            if (triangle_lum[i] > 1e-5f) {
                input_nodes[input_nodes_count++] = func(i);
            }
        },
        128);
    input_nodes.resize_uninitialized(input_nodes_count);
    auto bvh_result = BVH::build(
        r.nodes,
        input_nodes);
    LUISA_ASSERT(r.nodes.size() > 0);
    r.contribute = r.nodes[0].lum;
    r.bounding = bvh_result.bound;
    return r;
}

auto MeshLightAccel::build_bvh(
    Device &device,
    CommandList &cmdlist,
    BindlessArray &heap,
    BindlessArray &image_heap,
    RC<DeviceMesh> const &mesh,
    uint mat_index,
    uint mat_idx_buffer_heap_idx,
    TexStreamManager *tex_stream_mng,
    float4x4 transform) -> fiber::future<HostResult> {
    LUISA_ASSERT(!mesh->host_data().empty(), "Mesh must contained host data.");
    auto &meta = mesh->mesh_data()->meta;
    uint triangle_count = (mesh->host_data().size_bytes() - meta.tri_byte_offset) / sizeof(Triangle);
    fiber::future<HostResult> r;
    vector<float> lums;
    lums.push_back_uninitialized(triangle_count);
    auto lum_buffer = device.create_buffer<float>(triangle_count);
    cmdlist
        << (*_estimate_mesh)(
               tex_stream_mng->level_buffer(),
               heap,
               ByteBufferView{mesh->mesh_data()->pack.data},
               image_heap,
               meta.submesh_heap_idx,
               meta.vertex_count,
               meta.tri_byte_offset,
               meta.ele_mask, mat_index, mat_idx_buffer_heap_idx, lum_buffer)
               .dispatch(lum_buffer.size())
        << lum_buffer.view().copy_to(lums.data());
    cmdlist.add_callback(
        [lum_buffer = std::move(lum_buffer),
         mesh,
         transform,
         lums = std::move(lums),
         this,
         r]() mutable {
            _tasks.push(
                transform,
                std::move(mesh),
                std::move(lums),
                std::move(r));
        });
    return r;
}

void MeshLightAccel::update() {
    while (auto r = _tasks.pop()) {
        fiber::schedule([this, r = std::move(*r)]() mutable {
            auto &meta = r.mesh->mesh_data()->meta;
            uint vertex_count = meta.vertex_count;
            uint triangle_count = (r.mesh->host_data().size_bytes() - meta.tri_byte_offset) / sizeof(Triangle);
            auto host_data = r.mesh->host_data();
            auto result = build_bvh(
                r.transform,
                luisa::span{
                    (float3 *)host_data.data(),
                    vertex_count},
                luisa::span{
                    (Triangle *)(host_data.data() + meta.tri_byte_offset),
                    triangle_count},
                r.lums);
            r.result.signal(std::move(result));
        });
    }
}

void MeshLightAccel::create_or_update_blas(
    CommandList &cmdlist,
    Buffer<BVH::PackedNode> &buffer,
    vector<BVH::PackedNode> &&nodes) {
    LUISA_ASSERT(!nodes.empty());
    if (buffer && buffer.size() != nodes.size()) {
        SceneManager::instance().dispose_queue().dispose_after_queue(std::move(buffer));
    }
    if (!buffer) {
        buffer = RenderDevice::instance().lc_device().create_buffer<BVH::PackedNode>(nodes.size());
    }
    auto to_float3 = [](std::array<float, 3> a){
        return float3(a[0], a[1], a[2]);
    };
    cmdlist << buffer.view(0, nodes.size()).copy_from(nodes.data());
    SceneManager::instance().dispose_after_commit(std::move(nodes));
}

MeshLightAccel::MeshLightAccel() {
    ShaderManager::instance()->load("combine_mesh/estimate_mesh.bin", _estimate_mesh);
}
}// namespace rbc
