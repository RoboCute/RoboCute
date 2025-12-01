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

private:
    Shader1D<
        Buffer<uint>, // global level buffer
        BindlessArray,// &heap,
        ByteBuffer, // &mesh_buffer,
        BindlessArray,//&image_heap,
        // mesh meta
        uint,//submesh_heap_idx,
        uint,//vertex_count,
        uint,//tri_byte_offset,
        uint,//ele_mask,

        uint,//mat_index,
        uint,//mat_idx_buffer_heap_idx,
        Buffer<float>> const *_estimate_mesh;
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
        CommandList &cmdlist,
        Buffer<BVH::PackedNode> &buffer,
        vector<BVH::PackedNode> &&nodes);
    HostResult build_bvh(
        float4x4 matrix,
        span<float3 const> vertices,
        span<Triangle const> triangles,
        span<float const> triangle_lum);
    fiber::future<HostResult> build_bvh(
        Device &device,
        CommandList &cmdlist,
        BindlessArray &heap,
        BindlessArray &image_heap,
        RC<DeviceMesh> const &mesh,
        uint mat_index,
        uint mat_idx_buffer_heap_idx,
        TexStreamManager *tex_stream_mng,
        float4x4 transform);

    void update();
};
}// namespace rbc