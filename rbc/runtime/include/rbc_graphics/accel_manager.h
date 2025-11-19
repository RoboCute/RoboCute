#pragma once
#include <rbc_config.h>
#include <rbc_graphics/mesh_manager.h>
#include <rbc_graphics/buffer_allocator.h>
#include <rbc_graphics/buffer_uploader.h>
#include <luisa/runtime/device.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/raster/raster_state.h>
#include <luisa/runtime/rtx/procedural_primitive.h>
#include <luisa/vstl/lockfree_array_queue.h>
#include <rbc_graphics/mat_code.h>
namespace rbc
{
#include <geometry/types.hpp>
#include <geometry/procedural_types.hpp>
struct ShaderManager;
struct RBC_RUNTIME_API AccelManager {
public:
    using RasterElement = geometry::RasterElement;
    using InstanceInfo = geometry::InstanceInfo;
    using ProceduralType = geometry::ProceduralType;
    using DrawListMap = luisa::vector<RasterMesh>;
    struct AccelElement {
        float4x4 transform;
        vstd::variant<MeshManager::MeshData*, ProceduralPrimitive> mesh_data;
        uint user_id;
        uint8_t visibility_mask;
        bool opaque;
        AccelElement(
            float4x4 const& transform,
            uint8_t visibility_mask,
            bool opaque,
            MeshManager::MeshData* mesh_data,
            uint user_id
        )
            : transform(transform)
            , mesh_data(mesh_data)
            , visibility_mask(visibility_mask)
            , opaque(opaque)
            , user_id(user_id)
        {
        }
        AccelElement(
            float4x4 const& transform,
            uint8_t visibility_mask,
            ProceduralPrimitive&& mesh_data,
            uint user_id
        )
            : transform(transform)
            , mesh_data(std::move(mesh_data))
            , visibility_mask(visibility_mask)
            , opaque(false)
            , user_id(user_id)
        {
        }
        AccelElement(AccelElement&&) = default;
        AccelElement& operator=(AccelElement&&) = default;
        ~AccelElement() = default;
    };

private:
    Shader1D<Accel, Buffer<float4x4>> const* _update_last_transform{ nullptr };
    Shader1D<Accel, Buffer<float4x4>, float3> const* _move_the_world{ nullptr };
    Shader1D<Buffer<float4x4>, float3> const* _move_the_world_onlylast{ nullptr };
    Device& _device;
    Accel _accel;
    ////////////////////// Raster
    struct MeshInstanceList {
        luisa::spin_mutex mtx;
        vstd::fixed_vector<vstd::vector<uint>, 1> instance_indices;
    };
    using MeshMap = vstd::HashMap<MeshManager::MeshData*, MeshInstanceList>;
    vstd::LockFreeArrayQueue<MeshMap> _cache_maps;
    Buffer<RasterElement> _raster_transform_buffer;
    MeshFormat _basic_foramt;

    struct Instance {
        BufferAllocator::Node material_node;
        uint accel_id{ ~0u - 1u };
    };
    struct ProceduralInstance {
        BufferAllocator::Node meta_node;
        uint accel_id{ ~0u - 1u };
    };

    using ProceduralVariant = vstd::variant<
        geometry::HeightMap,
        geometry::VoxelSurface,
        geometry::SDFMap>;

    Buffer<ProceduralType> _procedural_type_buffer;
    Buffer<InstanceInfo> _inst_buffer;
    Buffer<AABB> _default_aabb_buffer;
    ProceduralPrimitive _default_procedural_prim;
    Buffer<float4x4> _last_trans_buffer;
    Buffer<uint> _triangle_vis_buffer; // copntained vis buffer heap idx
    vector<Instance> _insts;
    vector<ProceduralInstance> _procedural_insts;
    vector<uint> _user_id_pool;
    vector<uint> _procedural_user_id_pool;
    vector<AccelElement> _accel_elements;
    bool _dirty{ false };
    bool _accel_builded{ false };
    void _init_default_procedural_blas(
        CommandList& cmdlist,
        DisposeQueue& disp_queue
    );
    void _update_mesh_instance(
        uint inst_id,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        MeshManager::MeshData* mesh_data,
        span<const MatCode> mat_codes,
        float4x4 const& transform,
        uint8_t visibility_mask,
        bool opaque,
        uint light_id,
        uint8_t light_mask,
        bool reset_last = true,
        uint last_vert_buffer_id = -1,
        uint vis_buffer_heap_idx = -1
    );
    void _update_procedural_instance(
        uint inst_id,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        ProceduralVariant&& prim_data
    );
    void _swap_last(BufferUploader& uploader, auto& inst, DisposeQueue* disp_queue);

public:
    [[nodiscard]] auto const& procedural_type_buffer() const { return _procedural_type_buffer; }
    [[nodiscard]] auto const& inst_buffer() const { return _inst_buffer; }
    [[nodiscard]] auto const& last_trans_buffer() const { return _last_trans_buffer; }
    [[nodiscard]] auto const& accel() const { return _accel; }
    [[nodiscard]] auto& accel() { return _accel; }
    AccelManager(Device& device);
    void load_shader(luisa::fiber::counter& counter);
    void set_mesh_instance(
        CommandList& cmdlist,
        BufferUploader& uploader,
        uint inst_id,
        float4x4 transform,
        uint8_t visibility_mask,
        bool opaque
    );
    void set_procedural_instance(
        uint inst_id,
        float4x4 transform,
        uint8_t visibility_mask,
        bool opaque
    );

    [[nodiscard]] uint emplace_mesh_instance(
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        MeshManager::MeshData* mesh_data,
        span<const MatCode> mat_codes,
        float4x4 const& transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = true,
        uint light_id = 0,
        uint8_t light_mask = std::numeric_limits<uint8_t>::max(),
        uint last_vert_buffer_id = -1,
        uint vis_buffer_heap_idx = -1
    );
    [[nodiscard]] uint emplace_procedural_instance(
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        ProceduralPrimitive&& prim,
        ProceduralVariant&& prim_data,
        float4x4 const& transform,
        uint8_t visibility_mask = 0xffu
    );
    void set_mesh_instance(
        uint inst_id,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        MeshManager::MeshData* mesh_data,
        span<const MatCode> mat_codes,
        float4x4 const& transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = true,
        uint light_id = 0,
        uint8_t light_mask = std::numeric_limits<uint8_t>::max(),
        bool reset_last = false,
        uint last_vert_buffer_id = -1,
        uint vis_buffer_heap_idx = -1
    );
    void set_procedural_instance(
        uint inst_id,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        ProceduralPrimitive&& prim,
        ProceduralVariant&& prim_data,
        float4x4 const& transform,
        uint8_t visibility_mask = 0xffu
    );
    void init_accel(CommandList& cmdlist);

    void move_the_world(
        CommandList& cmdlist,
        float3 offset
    );

    [[nodiscard]] AccelElement const* try_get_accel_element(uint inst_id) const;

    void remove_mesh_instance(
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        uint inst_idx
    );
    void remove_procedural_instance(
        BufferAllocator& buffer_allocator,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        uint inst_idx
    );

    void build_accel(CommandList& cmdlist);
    void update_last_transform(
        CommandList& cmdlist
    );
    void mark_dirty()
    {
        _dirty = true;
    }
    BufferView<uint> triangle_vis_buffer() const
    {
        return _triangle_vis_buffer;
    }
    ~AccelManager();
    ////////////////////// Raster
    MeshFormat const& basic_foramt() const
    {
        return _basic_foramt;
    }
    void make_draw_list(
        CommandList& cmdlist,
        DisposeQueue& after_commit_dispqueue,
        DisposeQueue& after_sync_dispqueue,
        vstd::function<bool(float4x4 const&, AABB const&)> const& cull_func,
        DrawListMap& out_draw_meshes,
        BufferView<RasterElement>& out_data_buffer
    );
};
} // namespace rbc