#pragma once
#include "rbc_anim/render/render_data.h"
#include "rbc_anim/render/skelmesh_render.h"

namespace rbc {

struct SkelMeshRenderDataLODCPU : public SkelMeshRenderDataLOD {
};

struct RBC_RUNTIME_API SkeletalMeshRenderObjectCPUSkin : public SkeletalMeshRenderObject {

public:
    explicit SkeletalMeshRenderObjectCPUSkin(const SkeletalMesh *InSkelMesh);
    explicit SkeletalMeshRenderObjectCPUSkin(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc);
    virtual ~SkeletalMeshRenderObjectCPUSkin();

public:
    // 所有跟渲染相关的动态资产
    // 在RenderThread->CreateRenderState_ConCurrent初始化SkeletalMeshRenderObject的时候被InitResource初始化
    // 每一个Runtime SkeletalMesh都持有一个
    // 主要存储dynamic vertex buffers, SkinPrimitives和对应的GPU资产结构

public:// interface
    [[nodiscard]] void InitResources(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc) override;
    [[nodiscard]] void ReleaseResources() override;
    [[nodiscard]] void Update(AnimRenderState &state, int32_t LODIndex, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin) override;

private:
    void UpdateDynamicData_RenderThread();
    /**
     * CPUSkinAndCacheVertices
     * =============================================
     * * 执行CPUSkin
     * * 将CPU数据上传到GPU
     * * 替换RenderMesh中的primitive commands
     */
    void CPUSkinAndCacheVertices();

    /**
     * UpdateDrawcalls
     * ==============================================
     * * 更新光栅所需要的Drawcall数据
     */
    void UpdateDrawcalls();
    /**
     * UpdateGPUScene
     * ==============================================
     * * 更新光线追踪GPUScene所需要的BLAS更新
     */
    void UpdateGPUScene();

private:
    // RenderData for each LOD
    struct SkeletalMeshRenderDataLOD {
        SkeletalMeshRenderData *render_data_;
        int32_t lod_index_;
    };

private:
    // cached
    mutable int32_t cached_vertex_lod_;
    SkelMeshRenderDataLODCPU LOD;
};

}// namespace rbc
