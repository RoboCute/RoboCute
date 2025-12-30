#include "rbc_world/components/skelmesh_component.h"
#include "rbc_world/type_register.h"
#include "rbc_anim/skeletal_mesh.h"
#include "rbc_graphics/device_assets/device_transforming_mesh.h"
#include "rbc_graphics/render_device.h"

namespace rbc::world {

SkelMeshComponent::SkelMeshComponent(Entity *entity) : ComponentDerive<SkelMeshComponent>(entity) {}
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
    // Now Everything Ready, start Initialize RenderState
    if (!runtime_skel_mesh->RenderStateCreated()) {
        runtime_skel_mesh->CreateRenderState_Concurrent();
        auto &morph_data = morph_mesh->device_transforming_mesh()->mesh_data()->pack.mutable_data;
        morph_bytes.resize_uninitialized(morph_data.size_bytes());
        render_device->lc_main_cmd_list() << morph_data.view().copy_to(morph_bytes.data());

    } else {
        AnimRenderState state;
        // runtime_skel_mesh->DoDeferredRenderUpdate_Concurrent(state);

        auto *origin_mesh = morph_mesh->origin_mesh();
        auto *host_data = origin_mesh->host_data();

        auto &morph_data = morph_mesh->device_transforming_mesh()->mesh_data()->pack.mutable_data;

        auto vert_count = origin_mesh->vertex_count();

        luisa::span<float3> pos_{(float3 *)host_data->data(), vert_count};
        luisa::span<float3> target_pos_{(float3 *)morph_bytes.data(), vert_count};

        for (auto i = 0; i < vert_count; i++) {
            target_pos_[i].x = pos_[i].x + sin(time);
        }
        render_device->lc_main_cmd_list() << morph_data.view().copy_from(morph_bytes.data());
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

DECLARE_WORLD_COMPONENT_REGISTER(SkelMeshComponent);

}// namespace rbc::world