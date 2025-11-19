#include <rbc_graphics/light_type.h>
#include <rbc_graphics/mesh_light_accel.h>
#include <luisa/runtime/device.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
namespace rbc
{
auto MeshLightAccel::build_bvh(
    span<float3 const> vertices,
    span<Triangle const> triangles,
    span<float const> triangle_lum
) -> HostResult
{
    HostResult r;

    r.nodes.clear();
    vector<BVH::InputNode> input_nodes;
    auto func = [&](size_t i) {
        BVH::InputNode r;
        auto&& tri = triangles[i];
        float3 p0 = vertices[tri.i0];
        float3 p1 = vertices[tri.i1];
        float3 p2 = vertices[tri.i2];
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
    input_nodes.reserve(triangles.size());
    for (auto i : vstd::range(triangles.size()))
    {
        if (triangle_lum[i] > 1e-5f)
        {
            input_nodes.emplace_back(func(i));
        }
    }
    auto bvh_result = BVH::build(
        r.nodes,
        input_nodes
    );
    r.contribute = r.nodes[0].lum;
    r.bounding = bvh_result.bound;
    return r;
}

auto MeshLightAccel::build_bvh(
    Device& device,
    CommandList& cmdlist,
    BindlessArray& heap,
    BindlessArray& image_heap,
    MeshManager::MeshMeta const& mesh_meta,
    uint mat_index,
    uint mat_idx_buffer_heap_idx,
    BufferView<uint> mesh_data_buffer,
    uint tri_offset_uint,
    uint vertex_count,
    TexStreamManager* tex_stream_mng
) -> fiber::future<HostResult>
{
    fiber::future<HostResult> r;
    vector<float3> poses;
    vector<Triangle> triangles;
    vector<float> lums;
    poses.push_back_uninitialized(vertex_count);
    triangles.push_back_uninitialized((mesh_data_buffer.size() - tri_offset_uint) / 3);
    lums.push_back_uninitialized(triangles.size());
    auto lum_buffer = device.create_buffer<float>(triangles.size());
    assert(mesh_data_buffer.size() - tri_offset_uint == triangles.size() * 3);
    cmdlist
        << (*_estimate_mesh)(tex_stream_mng->level_buffer(), heap, image_heap, mesh_meta, mat_index, mat_idx_buffer_heap_idx, lum_buffer).dispatch(lum_buffer.size())
        << mesh_data_buffer.subview(0, vertex_count * sizeof(float3) / sizeof(uint)).copy_to(poses.data())
        << mesh_data_buffer.subview(tri_offset_uint, triangles.size() * 3).copy_to(triangles.data())
        << lum_buffer.view().copy_to(lums.data());
    cmdlist.add_callback(
        [lum_buffer = std::move(lum_buffer),
         poses = std::move(poses),
         triangles = std::move(triangles),
         lums = std::move(lums),
         this,
         r]() mutable {
            auto s = poses.size();
            auto b = triangles.size();
            _tasks.push(
                std::move(poses),
                std::move(triangles),
                std::move(lums),
                std::move(r)
            );
        }
    );
    return r;
}

void MeshLightAccel::update()
{
    while (auto r = _tasks.pop())
    {
        fiber::schedule([this, r = std::move(*r)]() mutable {
            auto result = build_bvh(
                r.poses,
                r.triangles,
                r.lums
            );
            r.result.signal(std::move(result));
        });
    }
}

void MeshLightAccel::create_or_update_blas(
    CommandList& cmdlist,
    Buffer<BVH::PackedNode>& buffer,
    vector<BVH::PackedNode>&& nodes
)
{
    LUISA_ASSERT(!nodes.empty());
    if (buffer && buffer.size() != nodes.size() * 2)
    {
        SceneManager::instance().dispose_queue().dispose_after_queue(std::move(buffer));
    }
    if (!buffer)
    {
        buffer = RenderDevice::instance().lc_device().create_buffer<BVH::PackedNode>(nodes.size() * 2);
    }
    cmdlist << buffer.view(nodes.size(), nodes.size()).copy_from(nodes.data());
    SceneManager::instance().dispose_after_commit(std::move(nodes));
}

void MeshLightAccel::update_blas_transform(
    CommandList& cmdlist,
    BufferView<BVH::PackedNode> nodes,
    uint node_count,
    float4x4 transform
)
{
    LUISA_ASSERT(nodes.size() == node_count * 2);
    cmdlist << (*_node_local_to_global)(
                   nodes,
                   transform
    )
                   .dispatch(node_count);
}
MeshLightAccel::MeshLightAccel()
{
    ShaderManager::instance()->load("combine_mesh/estimate_mesh.bin", _estimate_mesh);
    ShaderManager::instance()->load("combine_mesh/global_light_blas.bin", _node_local_to_global);
}
} // namespace rbc
