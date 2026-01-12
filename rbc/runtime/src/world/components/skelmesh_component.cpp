#include "rbc_world/components/skelmesh_component.h"
#include "rbc_world/type_register.h"
#include "rbc_anim/skeletal_mesh.h"
#include "rbc_graphics/device_assets/device_transforming_mesh.h"
#include "rbc_graphics/render_device.h"
#include "rbc_world/entity.h"

namespace rbc::world {

SkelMeshComponent::SkelMeshComponent() {}
SkelMeshComponent::~SkelMeshComponent() {
}
void SkelMeshComponent::on_awake() {
    // called when entity->_add_component was called
    runtime_skel_mesh = RC<SkeletalMesh>::New();
}
void SkelMeshComponent::on_destroy() {
    // called when Component->_clear_entity() was called
    remove_object();
}

void SkelMeshComponent::serialize_meta(ObjSerialize const &ser) const {
}
void SkelMeshComponent::deserialize_meta(ObjDeSerialize const &ser) {}

void SkelMeshComponent::tick(float delta_time) {
    // LUISA_INFO("Updating SkelMesh");
    if (!runtime_skel_mesh) [[unlikely]] {
        LUISA_ERROR("Ticking A SkelMeshComponent with an Invalid Runtime SkelMesh");
        return;
    }
    if (!_skel_mesh_ref->loaded()) [[unlikely]] {
        LUISA_ERROR("Ticking A SkelMeshComponent with an Unloaded Static SkelMesh Resource");
        return;
    }

    // Now Everything Ready, start Initialize
    if (!runtime_skel_mesh->IsInitialized()) {
        runtime_skel_mesh->ref_skelmesh = _skel_mesh_ref;
        runtime_skel_mesh->InitAnim();
        runtime_skel_mesh->EnableAnimation();// Start Ticking!
    }

    // Now Initialized, Start Tick
    runtime_skel_mesh->Tick(delta_time);
}

void SkelMeshComponent::update_render() {
    // LUISA_INFO("UpdateRender SkelMesh");
    if (!runtime_skel_mesh) [[unlikely]] {
        LUISA_ERROR("RenderTicking A SkelMeshComponent with an Invalid Runtime SkelMesh");
        return;
    }
    if (!_skel_mesh_ref->loaded()) [[unlikely]] {
        LUISA_ERROR("RenderTicking A SkelMeshComponent with an Unloaded Static SkelMesh Resource");
        return;
    }
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) [[unlikely]] {
        LUISA_ERROR("Calling SkelMeshComponent update_render with INVALID render_device");
        return;
    }
    auto *render = entity()->get_component<RenderComponent>();

    // Now Everything Ready, start Initialize RenderState
    if (!runtime_skel_mesh->RenderStateCreated()) {

        runtime_skel_mesh->CreateRenderState_Concurrent(render_device);
        StartUpdateRender(*render, bind_mats);
    } else {
        AnimRenderState state;
        runtime_skel_mesh->DoDeferredRenderUpdate_Concurrent(state);
    }
}

void SkelMeshComponent::remove_object() {

    if (!runtime_skel_mesh) {
        return;
    }
    if (runtime_skel_mesh->RenderStateCreated()) {
        runtime_skel_mesh->DestroyRenderState_Concurrent();
    }
    if (runtime_skel_mesh->IsInitialized()) {
        runtime_skel_mesh->DestroyAnim();
    }
    runtime_skel_mesh.reset();
}

void SkelMeshComponent::StartUpdateRender(RenderComponent &render, luisa::span<RC<MaterialResource> const> mats) {
    // auto &ref_mesh = _skel_mesh_ref->ref_skin->ref_mesh;
    auto &render_data = runtime_skel_mesh->GetRenderObject().GetLODRenderData();
    render.update_object(
        mats, render_data.morph_mesh.get());
}

MeshResource *SkelMeshComponent::GetRuntimeMesh() const {
    if (!runtime_skel_mesh->RenderStateCreated()) {
        LUISA_INFO("RenderState Not Created ~");
        return nullptr;
    }
    auto &render_data = runtime_skel_mesh->GetRenderObject().GetLODRenderData();
    return render_data.morph_mesh.get();
}

DECLARE_WORLD_OBJECT_REGISTER(SkelMeshComponent);

}// namespace rbc::world