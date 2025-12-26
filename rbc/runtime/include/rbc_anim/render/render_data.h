#pragma once
#include "render_data.h"
#include "rbc_core/rc.h"
#include "rbc_core/blob.h"
#include "rbc_world/resources/mesh.h"
#include "rbc_anim/asset//reference_skeleton.h"
#include "rbc_anim/types.h"

namespace rbc {

struct RBC_RUNTIME_API SkinPrimitive {
    bool UseNormals() const { return b_use_normals; }
    bool UseTangents() const { return b_use_tangents; }

public:
    // CPU Vertex Buffer Entry for Dynamic Skin Data
    skr::VertexBufferEntry position;
    skr::VertexBufferEntry normals;
    skr::VertexBufferEntry tangents;

    // flags
    bool b_use_normals = false;
    bool b_use_tangents = false;

    rbc::span<skr::VertexBufferView> vertex_buffer_views;
    // CPU Vertex Data Entry for Static Mesh Data
    uint32_t vertex_count;
    [[sattr(serde_off)]]
    const skr::VertexBufferEntry *ref_position = nullptr;
    [[sattr(serde_off)]]
    const skr::VertexBufferEntry *ref_normals = nullptr;
    [[sattr(serde_off)]]
    const skr::VertexBufferEntry *ref_tangents = nullptr;
    [[sattr(serde_off)]]
    const skr::VertexBufferEntry *ref_joints = nullptr;
    [[sattr(serde_off)]]
    const skr::VertexBufferEntry *ref_weights = nullptr;
};
// Some Configurations for Render
// located in SkelMeshResource
struct RBC_RUNTIME_API SkeletalMeshRenderData {
public:
    void InitializeWithRefSkeleton(const ReferenceSkeleton &InRefSkeleton);

public:
    bool support_ray_tracing = false;
    bool use_dynamic_buffer = false;
    const world::MeshResource *static_mesh_;// for acquiring mesh buffers
    luisa::vector<BoneIndexType> required_bones;
};

struct AnimRenderState {
};

struct SkelMeshRenderDataLOD {
    virtual ~SkelMeshRenderDataLOD() {}
    virtual void Initialize(const world::MeshResource *InMeshResource);
    virtual void InitResources(SkeletalMeshRenderData *InRenderData);
    virtual void ReleaseResources();

    SkeletalMeshRenderData *render_data_;
    luisa::vector<AnimFloat4x4> skin_matrices;
    luisa::vector<skr::SkinPrimitive> skin_primitives;// skin primitives
    // SkeletalMesh需要一些动态更新的Vertex/Normal/Tangent数据，构成Buffer
    uint32_t skin_vertex_buffer_size;
    luisa::vector<RC<rbc::IBlob>> buffers;// CPU buffers to skinning
};

}// namespace rbc