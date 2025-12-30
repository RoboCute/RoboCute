#pragma once
#include "render_data.h"
#include "rbc_core/rc.h"
#include "rbc_core/blob.h"
#include "rbc_core/buffer.h"
#include "rbc_world/resources/mesh.h"
#include "rbc_anim/asset//reference_skeleton.h"
#include "rbc_anim/types.h"

#include "rbc_graphics/render_device.h"
namespace rbc {

struct RBC_RUNTIME_API SkinPrimitive {
    [[nodiscard]] bool UseNormals() const { return b_use_normals; }
    [[nodiscard]] bool UseTangents() const { return b_use_tangents; }

public:
    // CPU Vertex Buffer Entry for Dynamic Skin Data
    rbc::VertexBufferEntry position;
    rbc::VertexBufferEntry normals;
    rbc::VertexBufferEntry tangents;

    // flags
    bool b_use_normals = false;
    bool b_use_tangents = false;

    // rbc::span<rbc::VertexBufferView> vertex_buffer_views;
    // CPU Vertex Data Entry for Static Mesh Data
    uint32_t vertex_count;
    const rbc::VertexBufferEntry *ref_position = nullptr;
    const rbc::VertexBufferEntry *ref_normals = nullptr;
    const rbc::VertexBufferEntry *ref_tangents = nullptr;
    const rbc::VertexBufferEntry *ref_joints = nullptr;
    const rbc::VertexBufferEntry *ref_weights = nullptr;
};
// Some Configurations for Render
// located in SkelMeshResource
struct RBC_RUNTIME_API SkeletalMeshRenderData {
public:
    void InitializeWithRefSkeleton(const ReferenceSkeleton &InRefSkeleton);

public:
    bool support_ray_tracing = false;
    bool use_dynamic_buffer = false;
    world::MeshResource *static_mesh_;// for acquiring mesh buffers
    luisa::vector<BoneIndexType> required_bones;
};

struct AnimRenderState {
};

struct SkelMeshRenderDataLOD {
    virtual ~SkelMeshRenderDataLOD() {}
    virtual void Initialize(world::MeshResource *InMeshResource, RenderDevice *InDevice);
    virtual void InitResources(SkeletalMeshRenderData *InRenderData, RenderDevice *InDevice);
    virtual void ReleaseResources();

    SkeletalMeshRenderData *render_data_;
    luisa::vector<AnimFloat4x4> skin_matrices;
    luisa::vector<SkinPrimitive> skin_primitives;// skin primitives
    // SkeletalMesh需要一些动态更新的Vertex/Normal/Tangent数据，构成Buffer
    uint32_t skin_vertex_buffer_size;

    RC<world::MeshResource> morph_mesh;// the morphing mesh instance
    luisa::vector<std::byte> morph_bytes;
};

}// namespace rbc