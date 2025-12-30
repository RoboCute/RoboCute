#include "rbc_anim/render/render_data.h"
#include "rbc_graphics/device_assets/device_transforming_mesh.h"

namespace rbc {

void SkeletalMeshRenderData::InitializeWithRefSkeleton(const rbc::ReferenceSkeleton &InRefSkeleton) {

    required_bones.resize_uninitialized(InRefSkeleton.NumJoints());
    for (auto i = 0; i < InRefSkeleton.NumJoints(); ++i) {
        required_bones[i] = (BoneIndexType)i;
    }
    LUISA_INFO("Initialize SkelMeshRenderData with {} Bones", required_bones.size());
}

void SkelMeshRenderDataLOD::Initialize(world::MeshResource *InMeshResource, RenderDevice *InDevice) {
    morph_mesh = world::create_object<world::MeshResource>();
    morph_mesh->create_as_morphing_instance(InMeshResource);
    morph_mesh->init_device_resource();

    auto &morph_data = morph_mesh->device_transforming_mesh()->mesh_data()->pack.mutable_data;
    morph_bytes.resize_uninitialized(morph_data.size_bytes());
    InDevice->lc_main_cmd_list() << morph_data.view().copy_to(morph_bytes.data());
}

void SkelMeshRenderDataLOD::InitResources(SkeletalMeshRenderData *InRenderData, RenderDevice *InDevice) {
    // 动态，应该跟随RenderObject初始化
    LUISA_INFO("Initializing Resources for SkelMeshRenderData");
    auto *InMeshResource = InRenderData->static_mesh_;
    render_data_ = InRenderData;
    Initialize(InMeshResource, InDevice);
}

void SkelMeshRenderDataLOD::ReleaseResources() {
}

}// namespace rbc
