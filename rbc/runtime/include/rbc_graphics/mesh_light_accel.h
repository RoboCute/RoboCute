#pragma once
#include <rbc_config.h>
#include <rbc_graphics/bvh.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/mesh_manager.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <luisa/core/stl/vector.h>
#include <luisa/runtime/rtx/triangle.h>
#include <luisa/runtime/buffer.h>
#include <luisa/vstl/lockfree_array_queue.h>

namespace rbc {
using namespace luisa::compute;
struct RBC_RUNTIME_API MeshLightAccel {
public:
    struct HostResult {
        vector<BVH::PackedNode> nodes;
        BVH::Bounding bounding;
        float contribute;
    };
    struct Task {
        vector<BVH::PackedNode> nodes;
        BufferView<BVH::PackedNode> buffer;
    };
    vstd::LockFreeArrayQueue<Task> _upload_task;

public:
    MeshLightAccel();
    bool create_or_update_blas(
        CommandList& cmdlist,
        Buffer<BVH::PackedNode> &buffer,
        vector<BVH::PackedNode> &&nodes);
    static HostResult build_bvh(
        float4x4 matrix,
        span<float3 const> vertices,
        span<Triangle const> triangles,
        span<uint const> submesh_offset,
        span<float const> submesh_lum);
    void update_frame(IOCommandList &io_cmdlist);
};
}// namespace rbc