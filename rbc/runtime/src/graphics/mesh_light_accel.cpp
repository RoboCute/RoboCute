#include <rbc_graphics/light_type.h>
#include <rbc_graphics/mesh_light_accel.h>
#include <luisa/runtime/device.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_core/utils/binary_search.h>
#include <rbc_core/atomic.h>
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
    std::array<std::atomic<float>, 6> bounding_min_max;
    for (auto i : vstd::range(3)) {
        bounding_min_max[i] = 1e20f;
        bounding_min_max[i + 3] = -1e20f;
    }
    std::atomic<float> contribute{0};
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
        for (auto i : vstd::range(3)) {
            atomic_min(bounding_min_max[i], r.bounding.min[i]);
            atomic_max(bounding_min_max[i + 3], r.bounding.max[i]);
        }
        float a = distance(p0, p1);
        float b = distance(p2, p1);
        float c = distance(p0, p2);
        float p = (a + b + c) * 0.5f;
        float area = std::sqrt(p * (p - a) * (p - b) * (p - c));
        auto contri = area * lum;
        r.bary_center_and_weight = make_float4((p0 + p1 + p2) / 3.f, contri);
        contribute += contri;
        auto plane_n = normalize(cross(p1 - p0, p2 - p0));
        r.cone = make_float4(0, 0, 1, pi);
        r.write_index(luisa::to_underlying(LightType::Triangle), i);
        return r;
    };
    size_t input_nodes_capacity{};
    for (auto submesh_index : vstd::range(submesh_lum.size())) {
        auto lum = submesh_lum[submesh_index];
        if (lum <= 1e-5f) {
            continue;
        }
        auto from = submesh_index < submesh_offset.size() ? submesh_offset[submesh_index] : 0;
        auto to = (submesh_index + 1) < submesh_offset.size() ? submesh_offset[submesh_index + 1] : triangles.size();
        input_nodes_capacity += to - from;
    }
    input_nodes.push_back_uninitialized(input_nodes_capacity);
    luisa::fiber::counter counter;
    std::atomic_size_t input_nodes_count{};
    for (auto submesh_index : vstd::range(submesh_lum.size())) {
        auto lum = submesh_lum[submesh_index];
        if (lum <= 1e-5f) {
            continue;
        }
        auto from = submesh_index < submesh_offset.size() ? submesh_offset[submesh_index] : 0;
        auto to = (submesh_index + 1) < submesh_offset.size() ? submesh_offset[submesh_index + 1] : triangles.size();
        luisa::fiber::async_parallel(
            counter,
            to - from,
            [&, from, lum](size_t i) {
                i = i + from;
                input_nodes[input_nodes_count++] = func(i, lum);
            },
            128);
    }
    counter.wait();
    input_nodes.resize_uninitialized(input_nodes_count);
    if (input_nodes_count == 0) {
        r.contribute = 0;
        r.bounding = {};
    } else {
        r.contribute = contribute;
        r.bounding.min = make_float3(
            bounding_min_max[0],
            bounding_min_max[1],
            bounding_min_max[2]);
        r.bounding.max = make_float3(
            bounding_min_max[3],
            bounding_min_max[4],
            bounding_min_max[5]);
    }
    r.buffer_size = input_nodes.size() * 2;
    r.nodes = luisa::fiber::async([input_nodes = std::move(input_nodes)]() {
        vector<BVH::PackedNode> nodes;
        auto bvh_result = BVH::build(
            nodes,
            input_nodes);
        return nodes;
    });

    return r;
}

bool MeshLightAccel::create_or_update_blas(
    CommandList &cmdlist,
    Buffer<BVH::PackedNode> &buffer,
    uint desired_buffer_size,
    luisa::fiber::future<vector<BVH::PackedNode>> &&nodes) {
    bool new_buffer{false};
    LUISA_ASSERT(desired_buffer_size > 0);
    if (buffer && buffer.size() != desired_buffer_size) {
        SceneManager::instance().dispose_after_sync(std::move(buffer));
    }
    if (!buffer) {
        buffer = RenderDevice::instance().lc_device().create_buffer<BVH::PackedNode>(desired_buffer_size);
        new_buffer = true;
    }
    _upload_task.push(std::move(nodes), buffer.view());
}

void MeshLightAccel::update_frame(IOCommandList &io_cmdlist) {
    while (auto p = _upload_task.pop()) {
        auto buffer_size = p->buffer.size_bytes();
        auto &&nodes = p->nodes.wait();
        io_cmdlist << IOCommand{
            nodes.data(),
            0,
            IOBufferSubView{p->buffer.subview(0, nodes.size())}};
        io_cmdlist.add_callback([n = std::move(nodes)] {});
    }
}

MeshLightAccel::MeshLightAccel() {
}
}// namespace rbc
