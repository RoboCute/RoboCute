#pragma once
#include <rbc_config.h>
#include <rbc_core/quaternion.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/command_list.h>
#include <luisa/runtime/shader.h>
#include <rbc_graphics/mesh_manager.h>
namespace rbc
{
struct ShaderManager;
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API Skinning {
    Shader1D<
    ByteBuffer,     // src_buffer,
    ByteBuffer,     // dst_buffer,
    Buffer<float3>, //& last_pos_buffer,

    Buffer<DualQuaternion>, //& dq_bone_buffer,// bone matrix in raw (4 colume, 3 raw)
    Buffer<uint>,                  // bone_indices,
    Buffer<float>,                 // bone_weights,
    uint,                          // bones_count_per_vert,
    uint,                          // vertex_count,
    bool,                          // contained_normal,
    bool,                          // contained_tangent,
    bool                           // reset_frame
    > const* _skinning{};

    void init(Device& device);
    Skinning() = default;
    Skinning(Skinning&&) = delete;
    Skinning(Skinning const&) = delete;
    Skinning& operator=(Skinning const&) = delete;
    Skinning& operator=(Skinning&&) = delete;
    ~Skinning();

    void update_mesh(
    CommandList& cmdlist,
    MeshManager::MeshData* out_mesh,
    MeshManager::MeshData* in_mesh,
    BufferView<DualQuaternion> dual_quat_skeleton,
    BufferView<float> skin_weights,
    BufferView<uint> skin_indices,
    BufferView<float3> last_poses,
    bool reset_frame);
};
} // namespace rbc