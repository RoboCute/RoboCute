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

namespace rbc
{
using namespace luisa::compute;
struct RBC_RUNTIME_API MeshLightAccel {
public:
    struct HostResult {
        vector<BVH::PackedNode> nodes;
        BVH::Bounding bounding;
        float contribute;
    };

private:
    Shader1D<Buffer<uint>, BindlessArray, BindlessArray, MeshManager::MeshMeta, uint, uint, Buffer<float>> const* _estimate_mesh;
    Shader1D<
        Buffer<BVH::PackedNode>, //& nodes,
        float4x4                 // local_to_global
        > const* _node_local_to_global;
    struct Task {
        float4x4 transform;
        RC<DeviceMesh> mesh;
        vector<float> lums;
        luisa::fiber::future<HostResult> result;
    };
    vstd::LockFreeArrayQueue<Task> _tasks;

public:
    MeshLightAccel();
    void create_or_update_blas(
        CommandList& cmdlist,
        Buffer<BVH::PackedNode>& buffer,
        vector<BVH::PackedNode>&& nodes
    );
    void update_blas_transform(
        CommandList& cmdlist,
        BufferView<BVH::PackedNode> nodes,
        uint node_count,
        float4x4 transform
    );
    HostResult build_bvh(
        float4x4 matrix,
        span<float3 const> vertices,
        span<Triangle const> triangles,
        span<float const> triangle_lum
    );
    fiber::future<HostResult> build_bvh(
        Device& device,
        CommandList& cmdlist,
        BindlessArray& heap,
        BindlessArray& image_heap,
        RC<DeviceMesh> const& mesh,
        uint mat_index,
        uint mat_idx_buffer_heap_idx,
        TexStreamManager* tex_stream_mng,
        float4x4 transform
    );

    void update();
};
} // namespace rbc