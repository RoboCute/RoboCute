#include "rbc_anim/render/skelmesh_render_cpu_skin.h"
#include "rbc_anim/types.h"
#include "ozz/base/span.h"
#include <tracy_wrapper.h>

namespace rbc {

void DoCPUSkinPrimitive(const SkinPrimitive &sk_prim, const SkelMeshRenderDataLODCPU *InRenderData, luisa::span<AnimFloat4x4> InReferenceToLocal) {
    auto *mesh = InRenderData->render_data_->static_mesh_;

    int vertex_count = sk_prim.vertex_count;
    // Fetch span from static mesh data
    auto static_buffer_span = [&vertex_count, &mesh](const VertexBufferEntry *buffer, auto t, uint32_t comps = 1) {
        using T = typename decltype(t)::type;
        LUISA_ASSERT(buffer->stride == sizeof(T) * comps);
        // auto offset = mesh->bins[buffer->buffer_index].blob->get_data() + buffer->offset;
        auto offset = 0;
        return ozz::span<const T>{(T *)offset, vertex_count * comps};
    };

    // Fetch span from dynamic data
    auto dynamic_buffer_span = [&vertex_count, &InRenderData](const VertexBufferEntry *buffer, auto t, uint32_t comps = 1) {
        using T = typename decltype(t)::type;
        LUISA_ASSERT(buffer->stride == sizeof(T) * comps);
        auto bin = InRenderData->buffers[buffer->buffer_index];
        auto offset = InRenderData->buffers[buffer->buffer_index]->get_data() + buffer->offset;
        return ozz::span<T>{(T *)offset, vertex_count * comps};
    };

    AnimSkinningJob job;
    // BASIC CONFIG

    job.vertex_count = sk_prim.vertex_count;
    job.joint_matrices = {InReferenceToLocal.data(), InReferenceToLocal.size()};
    job.vertex_count = vertex_count;
    job.influences_count = 4;

    // INPUT LAYOUT
    job.joint_indices = static_buffer_span(sk_prim.ref_joints, rbc::type_t<uint16_t>(), 4);
    job.joint_indices_stride = sk_prim.ref_joints->stride;
    // SKR_LOG_FMT_INFO(u8"Joint Indicies: {} with vertex count {}", job.joint_indices.size(), vertex_count);
    // for (auto i = 0; i < 10; i++)
    // {
    //     SKR_LOG_FMT_INFO(u8"Joint Index {}, {}, {}, {}", job.joint_indices[4 * i], job.joint_indices[4 * i + 1], job.joint_indices[4 * i + 2], job.joint_indices[4 * i + 3]);
    // }
    job.joint_weights = static_buffer_span(sk_prim.ref_weights, rbc::type_t<float>(), 4);
    job.joint_weights_stride = sk_prim.ref_weights->stride;
    // for (auto i = 0; i < 10; i++)
    // {
    //     SKR_LOG_FMT_INFO(u8"Joint Weight {}: {}", i, job.joint_weights[i]);
    // }
    if (sk_prim.UseNormals()) {
        job.in_normals = static_buffer_span(sk_prim.ref_normals, rbc::type_t<float>(), 3);
        job.in_normals_stride = sk_prim.ref_normals->stride;
    }

    if (sk_prim.UseTangents()) {
        job.in_tangents = static_buffer_span(sk_prim.ref_tangents, rbc::type_t<float>(), 4);
        job.in_tangents_stride = sk_prim.ref_tangents->stride;
    }

    job.in_positions = static_buffer_span(sk_prim.ref_position, rbc::type_t<float>(), 3);
    job.in_positions_stride = sk_prim.ref_position->stride;

    // OUTPUT LAYOUT

    job.out_positions = dynamic_buffer_span(&sk_prim.position, rbc::type_t<float>(), 3);
    job.out_positions_stride = sk_prim.position.stride;

    if (sk_prim.UseNormals()) {
        job.out_normals = dynamic_buffer_span(&sk_prim.normals, rbc::type_t<float>(), 3);
        job.out_normals_stride = sk_prim.normals.stride;
    }

    if (sk_prim.UseTangents()) {
        job.out_tangents = dynamic_buffer_span(&sk_prim.tangents, rbc::type_t<float>(), 4);
        job.out_tangents_stride = sk_prim.tangents.stride;
    }

    if (!job.Run()) {
        LUISA_ERROR("CPUSkinJob Failed");
    }
}

void DoCPUSkin(const SkelMeshRenderDataLODCPU *InRenderData, luisa::span<AnimFloat4x4> InReferenceToLocal) {
    for (auto i = 0; i < InRenderData->skin_primitives.size(); i++) {
        // create job per primitive
        DoCPUSkinPrimitive(InRenderData->skin_primitives[i], InRenderData, InReferenceToLocal);
    }
}

}// namespace rbc

namespace rbc {

SkeletalMeshRenderObjectCPUSkin::SkeletalMeshRenderObjectCPUSkin(const SkeletalMesh *InSkelMesh)
    : SkeletalMeshRenderObjectCPUSkin(SkeletalMeshSceneProxyDesc(InSkelMesh)) {
}

SkeletalMeshRenderObjectCPUSkin::SkeletalMeshRenderObjectCPUSkin(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc)
    : SkeletalMeshRenderObject(InSkelMeshDesc), cached_vertex_lod_(INVALID_INDEX) {

    InitResources(InSkelMeshDesc);
}

void SkeletalMeshRenderObjectCPUSkin::InitResources(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc) {
    LOD.InitResources(InSkelMeshDesc.render_data);
}

void SkeletalMeshRenderObjectCPUSkin::ReleaseResources() {
    LOD.ReleaseResources();
}

SkeletalMeshRenderObjectCPUSkin::~SkeletalMeshRenderObjectCPUSkin() {
}

/**
 * Update
 * @param LODIndex: 未来传入LODIndex的placeholder，从RenderData中找到对应LODIndex的渲染数据，暂时没有作用
 * @param InDynamicData: 动画采样完毕之后的ComponentSpaceBoneTransform入口
 * @param InRenderData: SkeletalMesh对应的渲染数据入口
 * 整个更新流程主要完成：
 * * 将ComponentSpaceBoneTransform批量乘InverseBindMatrix，得到蒙皮需要的ReferenceToLocal矩阵
 * * 将ReferenceToLocal矩阵和对应LOD的渲染数据进行CPU蒙皮，得到更新后的VertexBuffer
 * * 将更新后的VertexBuffer缓存并上传到GPU，方便渲染器调用
 */
void SkeletalMeshRenderObjectCPUSkin::Update(AnimRenderState &state, int32_t LODIndex, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin) {
    LUISA_INFO("Updating SkeletalMesh CPUSkin RenderObject");

    UpdateRefToLocalMatrices(LOD.skin_matrices, InDynamicData, InRefSkin);
    // 当前直接同步计算，未来会推入执行队列中异步计算
    UpdateDynamicData_RenderThread();
}

void SkeletalMeshRenderObjectCPUSkin::UpdateDynamicData_RenderThread() {
    CPUSkinAndCacheVertices();
    // override primitive commands
    if (false) {
        UpdateDrawcalls();
    }
    // override BLAS
    if (false) {
        UpdateGPUScene();
    }
}

void SkeletalMeshRenderObjectCPUSkin::CPUSkinAndCacheVertices() {
    LUISA_INFO("Doing CPUSkin");
    RBCZoneScopedN("DoCPUSkinAndUpload");
    DoCPUSkin(&LOD, LOD.skin_matrices);
    int buffer_size = LOD.skin_vertex_buffer_size;
}

void SkeletalMeshRenderObjectCPUSkin::UpdateDrawcalls() {
}

void SkeletalMeshRenderObjectCPUSkin::UpdateGPUScene() {}

}// namespace rbc