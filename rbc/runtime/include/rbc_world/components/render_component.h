#pragma once
#include <rbc_world/component.h>
#include <rbc_graphics/object_types.h>
#include <rbc_core/coroutine.h>

namespace rbc::world {
struct MeshResource;
struct MaterialResource;

struct RBC_RUNTIME_API RenderComponent final : ComponentDerive<RenderComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(RenderComponent)
private:
    RenderComponent(Entity *entity);
    ~RenderComponent();
    ObjectRenderType _type{};
    luisa::vector<MatCode> _material_codes;
    luisa::vector<RC<MaterialResource>> _materials;
    union {
        uint _mesh_tlas_idx;
        uint _mesh_light_idx;
        uint _procedural_idx;
    };

    RC<MeshResource> _mesh_ref;
    void _on_transform_update();
public:
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
    
    // draw
    uint get_tlas_index() const;
    void remove_object();
    void start_update_object(luisa::span<RC<MaterialResource> const> materials = {}, MeshResource *mesh = nullptr);
private:
    coro::coroutine _update_object(luisa::span<RC<MaterialResource> const> materials = {}, MeshResource *mesh = nullptr);
    void _update_object_pos(float4x4 matrix);
};
}// namespace rbc::world
RBC_RTTI(rbc::world::RenderComponent)