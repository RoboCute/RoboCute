#pragma once
#include "SkrAnim/objects/skeletal_mesh.hpp"
#include "SkrAnim/render/render_data.hpp"
#include "SkrAnim/render/skelmesh_render.hpp"
#include "SkrAnim/resources/skin_resource.hpp"
#include "SkrAnimCore/util.h"
#include "SkrRenderGraph/frontend/render_graph.hpp"

namespace skr
{

struct SkelMeshRenderDataLODCPU : public SkelMeshRenderDataLOD
{
};

struct SKR_ANIM_API SkeletalMeshRenderObjectCPUSkin : public SkeletalMeshRenderObject
{

public:
    SkeletalMeshRenderObjectCPUSkin(const SkeletalMesh* InSkelMesh, skr::RenderDevice* InRenderDevice);
    SkeletalMeshRenderObjectCPUSkin(const SkeletalMeshSceneProxyDesc& InSkelMeshDesc, skr::RenderDevice* InRenderDevice);
    virtual ~SkeletalMeshRenderObjectCPUSkin();

public:
    // 所有跟渲染相关的动态资产
    // 在RenderThread->CreateRenderState_ConCurrent初始化SkeletalMeshRenderObject的时候被InitResource初始化
    // 每一个Runtime SkeletalMesh都持有一个
    // 主要存储dynamic vertex buffers, SkinPrimitives和对应的GPU资产结构

public: // interface
    virtual void InitResources(const SkeletalMeshSceneProxyDesc& InSkelMeshDesc) override;
    virtual void ReleaseResources() override;
    virtual void Update(AnimRenderState& state, int32_t LODIndex, const SkeletalMeshSceneProxyDynamicData& InDynamicData, const SkinResource* InRefSkin) override;

private:
    void UpdateDynamicData_RenderThread(skr::RG::RenderGraph* rg);
    /**
     * CPUSkinAndCacheVertices
     * =============================================
     * * 执行CPUSkin
     * * 将CPU数据上传到GPU
     * * 替换RenderMesh中的primitive commands
     */
    void CPUSkinAndCacheVertices(skr::RG::RenderGraph* rg);

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
    struct SkeletalMeshRenderDataLOD
    {
        SkeletalMeshRenderData* render_data_;
        int32_t lod_index_;
    };

private:
    // cached
    mutable int32_t cached_vertex_lod_;
    SkelMeshRenderDataLODCPU LOD;
};

} // namespace skr
