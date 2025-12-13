#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/vector.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/component.h>
#include <rbc_core/quaternion.h>
namespace rbc::world {
struct Entity;
struct RBC_WORLD_API Transform final : ComponentDerive<Transform> {
    DECLARE_WORLD_COMPONENT_FRIEND(Transform)
    friend void clear_dirty_transform();
private:
    Transform *_parent{};
    luisa::unordered_set<Transform *> _children;
    double3 _position;
    double3 _scale{1, 1, 1};
    Quaternion _rotation;
    double4x4 _trs;
    bool _dirty : 1 {};
    bool _decomposed : 1 {true};
    Transform(Entity *entity) : ComponentDerive<Transform>(entity) {}
    void mark_dirty();
    void try_decompose();
    void traversal(double4x4 const &new_trs);
    ~Transform();
public:
    double3 position();
    Quaternion rotation();
    double3 scale();
    void serialize(ObjSerialize const&obj) const override;
    void deserialize(ObjDeSerialize const&obj) override;
    void dispose() override;
    void set_pos(double3 const &position, bool recursive);
    void set_rotation(Quaternion const &rotation, bool recursive);
    void set_scale(double3 const &scale, bool recursive);
    void set_trs(double4x4 const &trs, bool recursive);
    void set_trs(
        double3 const &position,
        Quaternion const &rotation,
        double3 const &scale,
        bool recursive);
    void add_children(Transform *tr);
    bool remove_children(Transform *tr);
    [[nodiscard]] auto const &position() const { return _position; }
    [[nodiscard]] auto const &rotation() const { return _rotation; }
    [[nodiscard]] auto const &scale() const { return _scale; }
    [[nodiscard]] auto const &children() const { return _children; }
    [[nodiscard]] double4x4 trs() const {
        return _trs;
    }
    [[nodiscard]] float4x4 trs_float() const;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Transform);