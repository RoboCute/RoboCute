#include "generated/world.h"
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light.h>

namespace rbc {
vstd::Guid Object::guid(void *this_) {
    return static_cast<world::BaseObject *>(this_)->guid();
}
uint64_t Object::instance_id(void *this_) {
    return static_cast<world::BaseObject *>(this_)->instance_id();
}
rbc::ResourceLoadStatus Resource::load_status(void *this_) {
    return static_cast<rbc::ResourceLoadStatus>(static_cast<world::Resource *>(this_)->loading_status());
}
bool Resource::loaded(void *this_) {
    return static_cast<world::Resource *>(this_)->loaded();
}
bool Resource::loading(void *this_) {
    return static_cast<world::Resource *>(this_)->loading_status() == world::EResourceLoadingStatus::Loading;
}
luisa::string Resource::path(void *this_) {
    return luisa::to_string(static_cast<world::Resource *>(this_)->path());
}
bool Resource::save_to_path(void *this_) {
    return static_cast<world::Resource *>(this_)->save_to_path();
}
void *Entity::_create_() {
    auto entity = world::create_object<world::Entity>();
    manually_add_ref(entity);
    return entity;
}
void *Entity::add_component(void *this_, luisa::string_view name) {
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    auto e = static_cast<world::Entity *>(this_);
    auto comp = e->_create_component(
        reinterpret_cast<std::array<uint64_t, 2> const &>(md5));
    if (!comp) return nullptr;
    e->_add_component(comp);
    return comp;
}
void Entity::_destroy_(void *ptr) {
    manually_release_ref(static_cast<world::BaseObject *>(ptr));
}
void *Entity::get_component(void *this_, luisa::string_view name) {
    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    auto comp = e->get_component(reinterpret_cast<std::array<uint64_t, 2> const &>(md5));
    return comp;
}
bool Entity::remove_component(void *this_, luisa::string_view name) {
    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    return e->remove_component(reinterpret_cast<std::array<uint64_t, 2> const &>(md5));
}
void *Component::entity(void *this_) {
    auto c = static_cast<world::Component *>(this_);
    return c->entity();
}
uint64_t TransformComponent::children_count(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->children().size();
}
luisa::double3 TransformComponent::position(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->position();
}
bool TransformComponent::remove_children(void *this_, void *children) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->remove_children(static_cast<world::TransformComponent *>(children));
}
luisa::float4 TransformComponent::rotation(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->rotation().v;
}
luisa::double3 TransformComponent::scale(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->scale();
}
void TransformComponent::set_pos(void *this_, luisa::double3 pos, bool recursive) {
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_pos(pos, recursive);
}
void TransformComponent::set_rotation(void *this_, luisa::float4 rotation, bool recursive) {
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_rotation(rotation, recursive);
}
void TransformComponent::set_scale(void *this_, luisa::double3 scale, bool recursive) {
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_scale(scale, recursive);
}
void TransformComponent::set_trs(void *this_, luisa::double3 pos, luisa::float4 rotation, luisa::double3 scale, bool recursive) {
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_trs(
        pos,
        Quaternion{rotation},
        scale,
        recursive);
}
void TransformComponent::set_trs_matrix(void *this_, luisa::double4x4 trs, bool recursive) {
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_trs(trs, recursive);
}
luisa::double4x4 TransformComponent::trs(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->trs();
}
luisa::float4x4 TransformComponent::trs_float(void *this_) {
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->trs_float();
}
void LightComponent::add_area_light(void *this_, luisa::float3 luminance, bool visible) {
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_area_light(luminance, visible);
}
void LightComponent::add_disk_light(void *this_, luisa::float3 luminance, bool visible) {
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_disk_light(luminance, visible);
}
void LightComponent::add_point_light(void *this_, luisa::float3 luminance, bool visible) {
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_point_light(luminance, visible);
}
void LightComponent::add_spot_light(void *this_, luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_spot_light(luminance, angle_radians, small_angle_radians, angle_atten_pow, visible);
}
float LightComponent::angle_atten_pow(void *this_) {
    auto c = static_cast<world::LightComponent *>(this_);
    return c->angle_atten_pow();
}
float LightComponent::angle_radians(void *this_) {
    auto c = static_cast<world::LightComponent *>(this_);
    return c->angle_radians();
}
luisa::float3 LightComponent::luminance(void *this_) {
    auto c = static_cast<world::LightComponent *>(this_);
    return c->luminance();
}
float LightComponent::small_angle_radians(void *this_) {
    auto c = static_cast<world::LightComponent *>(this_);
    return c->small_angle_radians();
}
}// namespace rbc