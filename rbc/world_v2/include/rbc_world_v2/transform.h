#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/vector.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/component.h>
#include <rbc_core/quaternion.h>
namespace rbc::world {
struct Entity;
struct TransformImpl;
struct WorldPluginImpl;
struct Transform : ComponentDerive<Transform> {
    friend struct TransformImpl;
    friend struct WorldPluginImpl;
protected:
    Transform *_parent{};
    luisa::vector<InstanceID> _children;
    double3 _position;
    double3 _scale{1, 1, 1};
    Quaternion _rotation;
    double4x4 _trs;
    bool _dirty: 1{};
    bool _decomposed: 1{true};

public:
    virtual double3 position() = 0;
    virtual Quaternion rotation() = 0;
    virtual double3 scale() = 0;
    virtual void set_pos(double3 const &position, bool recursive) = 0;
    virtual void set_rotation(Quaternion const &rotation, bool recursive) = 0;
    virtual void set_scale(double3 const &scale, bool recursive) = 0;
    virtual void set_trs(double4x4 const &trs, bool recursive) = 0;
    virtual void set_trs(
        double3 const &position,
        Quaternion const &rotation,
        double3 const &scale,
        bool recursive) = 0;
    virtual void add_children(Transform *tr) = 0;
    virtual void remove_children_at(uint64_t index) = 0;
    virtual bool remove_children(Transform *tr) = 0;
    [[nodiscard]] auto const &position() const { return _position; }
    [[nodiscard]] auto const &rotation() const { return _rotation; }
    [[nodiscard]] auto const &scale() const { return _scale; }
    [[nodiscard]] luisa::span<InstanceID const> children() const { return _children; }
    [[nodiscard]] double4x4 local_to_world_matrix() const {
        return _trs;
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Transform);