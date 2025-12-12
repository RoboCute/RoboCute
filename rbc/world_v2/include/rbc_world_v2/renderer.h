#pragma once
#include <rbc_world_v2/component.h>
#include <rbc_graphics/object_types.h>

namespace rbc::world {
struct Mesh;
struct Material;
struct RBC_WORLD_API Renderer final : ComponentDerive<Renderer> {
    DECLARE_WORLD_OBJECT_FRIEND(Renderer)
private:
    Renderer();
    ~Renderer();
    ObjectRenderType _type{};
    luisa::vector<MatCode> _material_codes;
    luisa::vector<RC<Material>> _materials;
    union {
        uint _mesh_tlas_idx;
        uint _mesh_light_idx;
        uint _procedural_idx;
    };

    RC<Mesh> _mesh_ref;
    void _on_transform_update();
public:
    void on_awake() override;
    void on_destroy() override;
    void rbc_objser(rbc::JsonSerializer &ser_obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
    void dispose() override;
    // draw
    void update_object(luisa::span<RC<Material> const> materials, Mesh *mesh = nullptr);
    uint get_tlas_index() const;
    void remove_object();
private:
    void _update_object_pos(float4x4 matrix);
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Renderer)