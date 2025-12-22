#include <rbc_graphics/light_type.h>
#include <rbc_graphics/mesh_light_accel.h>
#include <luisa/runtime/device.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_core/utils/binary_search.h>
namespace rbc {
auto MeshLightAccel::build_bvh(
    float4x4 matrix,
    span<float3 const> vertices,
    span<Triangle const> triangles,
    span<uint const> submesh_offset,
    span<float const> submesh_lum) -> HostResult {
    HostResult r;

    r.nodes.clear();
    vector<BVH::InputNode> input_nodes;
    auto func = [&](size_t i, float lum) {
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
        r.bary_center_and_weight = make_float4((p0 + p1 + p2) / 3.f, area * lum);
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
            uint submesh_index = 0;
            if (submesh_offset.size() > 0) {
                submesh_index = binary_search(
                                    submesh_offset,
                                    i + 1,
                                    [](auto a, auto b) {
                                        if (a < b) return -1;
                                        return a > b ? 1 : 0;
                                    }) -
                                1;
            }
            auto lum = submesh_lum[submesh_index];
            if (lum > 1e-5f) {
                input_nodes[input_nodes_count++] = func(i, lum);
            }
        },
        128);
    input_nodes.resize_uninitialized(input_nodes_count);
    auto bvh_result = BVH::build(
        r.nodes,
        input_nodes);
    if (r.nodes.empty()) {
        r.contribute = 0;
        r.bounding = {};
    } else {
        r.contribute = r.nodes[0].lum;
        r.bounding = bvh_result.bound;
    }
    return r;
}

bool MeshLightAccel::create_or_update_blas(
    CommandList &cmdlist,
    Buffer<BVH::PackedNode> &buffer,
    vector<BVH::PackedNode> &&nodes) {
    bool new_buffer{false};
    LUISA_ASSERT(!nodes.empty());
    if (buffer && buffer.size() != nodes.size()) {
        SceneManager::instance().dispose_after_sync(std::move(buffer));
    }
    if (!buffer) {
        buffer = RenderDevice::instance().lc_device().create_buffer<BVH::PackedNode>(nodes.size());
        new_buffer = true;
    }
    _upload_task.push(std::move(nodes), buffer.view());
}

void MeshLightAccel::update_frame(IOCommandList &io_cmdlist) {
    while (auto p = _upload_task.pop()) {
        auto buffer_size = p->buffer.size_bytes();
        io_cmdlist << IOCommand{
            p->nodes.data(),
            0,
            IOBufferSubView{p->buffer}};
        io_cmdlist.add_callback([n = std::move(p->nodes)] {});
    }
}

MeshLightAccel::MeshLightAccel() {
}
}// namespace rbc
