#pragma once

#include "rbc_world/resources/mesh.h"
#include "rbc_anim/render/render_data.h"
#include "rbc_world/resources/skelmesh.h"
#include "rbc_world/resources/mesh.h"
#include "rbc_world/resources/skin.h"

namespace rbc {
struct SkeletalMesh;
}// namespace rbc

namespace rbc {

// [PrimitiveSceneProxyDesc]
struct SkeletalMeshSceneProxyDesc {
    explicit SkeletalMeshSceneProxyDesc(const SkeletalMesh *InSkelMesh);

    SkinResource *skin_resource;
    world::MeshResource *mesh_resource;
    SkeletonResource *skel_resource;
    SkeletalMeshRenderData *render_data;
    SkelMeshResource *skelmesh_resource;
};

// 给Renderer的统一动态数据接口
struct SkeletalMeshSceneProxyDynamicData {
public:
    explicit SkeletalMeshSceneProxyDynamicData(const SkeletalMesh *InSkelMesh);
    SkeletalMeshSceneProxyDynamicData() = default;

public:
    [[nodiscard]] bool IsSkinCacheAllowed() const;
    [[nodiscard]] uint32_t GetBoneTransformRevisionNumber() const;
    [[nodiscard]] uint32_t GetPreviousBoneTransformRevisionNumber() const;
    [[nodiscard]] uint32_t GetCurrentBoneTransformFrame() const;
    [[nodiscard]] int32_t GetNumLODs() const;

    // Core Data
    [[nodiscard]] luisa::span<const AnimFloat4x4> GetComponentSpaceTransforms() const;
    [[nodiscard]] luisa::span<const AnimFloat4x4> GetPreviousComponentSpaceTransforms() const;
    // [WIP] BoneVisibilityState
    // [WIP] Component Transform ?
    // [WIP] RefPoseOverride
private:
    luisa::span<const AnimFloat4x4> component_space_transforms_;
};

/**
 * SkeletalMeshRenderObject
 * =============================================
 * 用于执行蒙皮/上传操作的核心结构
 */
struct RBC_RUNTIME_API SkeletalMeshRenderObject {
public:
    explicit SkeletalMeshRenderObject(const SkeletalMesh *InSkelMesh);
    explicit SkeletalMeshRenderObject(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc);
    virtual ~SkeletalMeshRenderObject();

public:
    virtual void InitResources(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc) = 0;
    virtual void ReleaseResources() = 0;

    /**
     * Update
     * @param LODIndex: 未来传入LODIndex的placeholder，从RenderData中找到对应LODIndex的渲染数据，暂时没有作用
     * @param InDynamicData: 动画采样完毕之后的ComponentSpaceBoneTransform入口
     * 整个更新流程主要完成：
     * * 将ComponentSpaceBoneTransform批量乘InverseBindMatrix，得到蒙皮需要的ReferenceToLocal矩阵
     * * 将ReferenceToLocal矩阵和对应LOD的渲染数据进行CPU/GPU蒙皮，得到更新后的VertexBuffer
     * * 将更新后的VertexBuffer缓存并上传到GPU，后续渲染器调用
     */
    virtual void Update(AnimRenderState &state, int32_t LODIndex, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin) = 0;

    virtual bool IsCPUSkinned() { return true; }
    virtual bool IsGPUSkinned() { return false; }

protected:
    uint32_t last_frame_number_;
};

// Utility Functions

/**
 * UpdateRefToLocalMatrices
 * ======================================
 * Utility Function That Compute
 * * ReferenceToLocal matrices
 required by the skinning procedural from
 * * Evaluated Bones (InDynamicData)
 * * Inverse Binding Matrices from Reference Skin Resource
 */
void UpdateRefToLocalMatrices(luisa::vector<AnimFloat4x4> &ReferenceToLocal, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin);

}// namespace rbc