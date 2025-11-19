#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/runtime/bindless_array.h>
#include "bindless_allocator.h"
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/aabb.h>
#include <luisa/backends/ext/dstorage_ext.hpp>
#include <luisa/core/fiber.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/pool.h>
#include <rbc_graphics/host_buffer_manager.h>
#include <rbc_graphics/dispose_queue.h>
namespace rbc
{
#include <geometry/types.hpp>
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct RBC_RUNTIME_API MeshManager {
public:
    using MeshMeta = geometry::MeshMeta;
    struct MeshPack {
        Buffer<uint> data;
        Buffer<uint> submesh_indices;
        Mesh mesh;
    };
    struct MeshData;
    struct BBoxRequest {
        luisa::fixed_vector<AABB, 1> bounding_box;
        std::atomic_bool finished{};
        MeshData* mesh_data;
    };
    struct MeshData {
        friend struct MeshManager;
        luisa::vector<uint> submesh_offset;
        luisa::shared_ptr<BBoxRequest> bbox_requests;
        MeshPack pack;
        uint triangle_size;
        MeshMeta meta;
        bool is_vertex_instance;
        MeshData(MeshData const&) = delete;
        MeshData(MeshData&&) = delete;
        RBC_RUNTIME_API void build_mesh(Device& device, CommandList& cmdlist, AccelOption const& option);

    private:
        MeshData() = default;
        vstd::Pool<MeshData, false>* pool;
        ~MeshData() = default;
    };

private:
    struct MeshDataPack {
        MeshData data;
        MeshDataPack() = default;
        ~MeshDataPack() = default;
    };
    vstd::Pool<MeshDataPack, false> pool;
    Device& device;
    Buffer<AABB> _aabb_cache_buffer;
    luisa::vector<luisa::shared_ptr<BBoxRequest>> _bounding_requests;
    luisa::spin_mutex _pool_mtx;
    luisa::spin_mutex _mtx;
    luisa::spin_mutex _bounding_mtx;
    luisa::vector<std::pair<MeshData*, AccelOption>> _build_cmds;
    luisa::vector<MeshData*> _unload_cmds;
    Shader1D<Buffer<uint>, Buffer<uint>> const* set_submesh{ nullptr };
    Shader1D<
        BindlessArray, //& buffer_heap,
        Buffer<uint2>, //& offset_buffer,// x: vertex_heap_idx  y: tri_element_offset
        Buffer<uint>,  //& result_aabb,
        bool           // clear
        > const* compute_bound{ nullptr };
    void _create_submesh_buffer(
        CommandList& cmdlist,
        BindlessAllocator& bdls_alloc,
        HostBufferManager& temp_buffer,
        MeshData* mesh_data
    );

public:
    // [[nodiscard]] auto const& get_mesh(uint mesh_idx) const {
    // 	return meshes[mesh_idx];
    // }
    MeshManager(Device& device);
    void load_shader(luisa::fiber::counter& counter);

    //////////////////// Async thread, thread safe
    MeshData* load_mesh(
        BindlessAllocator& bdls_mng,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        Buffer<uint>&& data_buffer,
        // Build this mesh?
        vstd::optional<AccelOption> option,
        uint vertex_count,
        bool normal,
        bool tangent,
        uint uv_count,
        // submesh range, first element must be 0
        vstd::span<uint const> submesh_triangle_offset,
        bool calculate_bounding = false
    );

    // for transforming, share data_buffer
    MeshData* make_transforming_instance(
        BindlessAllocator& bdls_alloc,
        CommandList& cmdlist,
        MeshData* mesh_data
    );

    void emplace_build_mesh_cmd(
        MeshData* mesh_data,
        AccelOption const& option
    );

    void emplace_unload_mesh_cmd(
        MeshData* mesh_data
    );

    void execute_build_cmds(CommandList& cmdlist, BindlessAllocator& bdls_alloc, HostBufferManager& temp_buffer);
    void on_frame_end(
        CommandList& cmdlist,
        BindlessAllocator& bdls_alloc
    );
    void execute_compute_bounding(
        CommandList& cmdlist,
        BindlessArray const& buffer_heap,
        HostBufferManager& temp_buffer,
        DisposeQueue& dsp_queue
    );
    //////////////////// Async thread
    ~MeshManager();
};
} // namespace rbc