#pragma once
#include <rbc_config.h>
#include <rbc_graphics/bvh.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/mesh_manager.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/accel_manager.h>
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
        vector<float3> poses;
        vector<Triangle> triangles;
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
        span<float3 const> vertices,
        span<Triangle const> triangles,
        span<float const> triangle_lum
    );
    fiber::future<HostResult> build_bvh(
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
    );

    void update();
};
} // namespace rbc