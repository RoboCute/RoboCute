#pragma once
#include <luisa/core/mathematics.h>
#include "rbc_world/component.h"
#include "rbc_world/resources/skelmesh.h"
#include "rbc_anim/skeletal_mesh.h"
#include "rbc_world/components/render_component.h"
namespace rbc::world {

struct RBC_RUNTIME_API SkelMeshComponent final : ComponentDerive<SkelMeshComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(SkelMeshComponent)

private:
    SkelMeshComponent();
    ~SkelMeshComponent();

    RC<SkelMeshResource> _skel_mesh_ref;// the animatable skeletal mesh resource
    RC<SkeletalMesh> _runtime_skel_mesh; // the runtime skeletal mesh


public:
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;

    void SetRefSkelMesh(RC<SkelMeshResource> &_skel_mesh) { _skel_mesh_ref = _skel_mesh; }

    float time = 0.0f;
    luisa::span<const RC<MaterialResource>> bind_mats;

    MeshResource *GetRuntimeMesh() const;
    bool IsEnabled() const;

    // Playback speed for animation
    float playback_speed = 1.0f;

public:
    void tick(float delta_time = 0.0f);
    void update_render();
    void remove_object();
    void _start_update_render(RenderComponent &render, luisa::span<RC<MaterialResource> const> mats);

    // Animation control
    void PlayAnimation();
    void PauseAnimation();
    void StopAnimation();

    // Bone transform access
    int GetNumBones() const;
    luisa::float4x4 GetBoneTransform(int bone_index) const;
    void SetBoneTransform(int bone_index, const luisa::float4x4 &transform);

    // Playback speed control
    float GetPlaybackSpeed() const { return playback_speed; }
    void SetPlaybackSpeed(float speed) { playback_speed = speed; }
};

}// namespace rbc::world

RBC_RTTI(rbc::world::SkelMeshComponent);