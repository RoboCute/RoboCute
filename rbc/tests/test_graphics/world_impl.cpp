#include "generated/world.h"
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_app/graphics_utils.h>

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
    if (!comp) {
        return nullptr;
    }
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

void *TextureResource::_create_() {
    auto p = world::create_object<world::TextureResource>();
    manually_add_ref(p);
    return p;
}
void *MeshResource::_create_() {
    auto p = world::create_object<world::MeshResource>();
    manually_add_ref(p);
    return p;
}
void *MaterialResource::_create_() {
    auto p = world::create_object<world::MaterialResource>();
    manually_add_ref(p);
    return p;
}
void TextureResource::_destroy_(void *ptr) {
    manually_release_ref(static_cast<world::BaseObject *>(ptr));
}
void MeshResource::_destroy_(void *ptr) {
    manually_release_ref(static_cast<world::BaseObject *>(ptr));
}
void MaterialResource::_destroy_(void *ptr) {
    manually_release_ref(static_cast<world::BaseObject *>(ptr));
}
void TextureResource::create_empty(void *this_, rbc::LCPixelStorage pixel_storage, luisa::uint2 size, uint32_t mip_level, bool is_virtual_texture) {
    auto c = static_cast<world::TextureResource *>(this_);
    c->create_empty(pixel_storage, size, mip_level, is_virtual_texture);
}
void TextureResource::upload(void *this_, uint mip_level) {
    auto c = static_cast<world::TextureResource *>(this_);
    if (c->is_vt()) [[unlikely]] {
        LUISA_ERROR("Can not upload to virtual-texture.");
    }
    auto tex = c->get_image();
    if (!tex) [[unlikely]] {
        LUISA_ERROR("Texture not initialized.");
    }
    GraphicsUtils::instance()->update_texture(tex, mip_level);
}
luisa::span<std::byte> TextureResource::data_buffer(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    auto ptr = c->host_data();
    if (!ptr) return {};
    if (ptr->empty()) {
        ptr->push_back_uninitialized(c->desire_size_bytes());
    }
    return *ptr;
}
bool TextureResource::has_data_buffer(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->host_data();
}
uint32_t TextureResource::heap_index(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->heap_index();
}
bool TextureResource::init_device_resource(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->init_device_resource();
}
bool TextureResource::is_vt(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->is_vt();
}
bool TextureResource::load_executed(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->load_executed();
}
uint32_t TextureResource::mip_level(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->mip_level();
}
bool TextureResource::pack_to_tile(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->pack_to_tile();
}
rbc::LCPixelStorage TextureResource::pixel_storage(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->pixel_storage();
}
luisa::uint2 TextureResource::size(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->size();
}
void MeshResource::upload(void *this_, bool only_vertex) {
    auto c = static_cast<world::MeshResource *>(this_);
    auto mesh = c->device_mesh();
    if (!mesh) [[unlikely]] {
        LUISA_ERROR("Can not upload to un-initialized mesh.");
    }
    GraphicsUtils::instance()->update_mesh_data(mesh, only_vertex);
}

uint64_t MeshResource::basic_size_bytes(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->basic_size_bytes();
}
bool MeshResource::contained_normal(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->contained_normal();
}
bool MeshResource::contained_tangent(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->contained_tangent();
}
void MeshResource::create_as_morphing_instance(void *this_, void *origin_mesh) {
    auto c = static_cast<world::MeshResource *>(this_);
    c->create_as_morphing_instance(static_cast<world::MeshResource *>(origin_mesh));
}
void MeshResource::create_empty(void *this_, luisa::span<std::byte> offset, uint32_t vertex_count, uint32_t triangle_count, uint32_t uv_count, bool contained_normal, bool contained_tangent) {
    luisa::vector<uint> submesh_offsets;
    submesh_offsets.push_back_uninitialized(offset.size_bytes() / sizeof(uint));
    std::memcpy(submesh_offsets.data(), offset.data(), submesh_offsets.size_bytes());
    auto c = static_cast<world::MeshResource *>(this_);
    c->create_empty(std::move(submesh_offsets), vertex_count, triangle_count, uv_count, contained_normal, contained_tangent);
}
luisa::span<std::byte> MeshResource::data_buffer(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) {
        data->push_back_uninitialized(c->basic_size_bytes());
    }
    return *data;
}
uint64_t MeshResource::desire_size_bytes(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->desire_size_bytes();
}
uint64_t MeshResource::extra_size_bytes(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->extra_size_bytes();
}
bool MeshResource::has_data_buffer(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->host_data();
}
bool MeshResource::init_device_resource(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->init_device_resource();
}
bool MeshResource::is_transforming_mesh(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->is_transforming_mesh();
}
uint32_t MeshResource::submesh_count(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->submesh_count();
}
uint32_t MeshResource::triangle_count(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->triangle_count();
}
uint32_t MeshResource::uv_count(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->uv_count();
}
uint32_t MeshResource::vertex_count(void *this_) {
    auto c = static_cast<world::MeshResource *>(this_);
    return c->vertex_count();
}
bool MaterialResource::init_device_resource(void *this_) {
    auto c = static_cast<world::MaterialResource *>(this_);
    return c->init_device_resource();
}
void MaterialResource::load_from_json(void *this_, luisa::string_view json) {
    auto c = static_cast<world::MaterialResource *>(this_);
    c->load_from_json(json);
}
uint32_t MaterialResource::mat_code(void *this_) {
    auto c = static_cast<world::MaterialResource *>(this_);
    return c->mat_code().value;
}

uint32_t RenderComponent::get_tlas_index(void *this_) {
    auto c = static_cast<world::RenderComponent *>(this_);
    return c->get_tlas_index();
}
void RenderComponent::remove_object(void *this_) {
    auto c = static_cast<world::RenderComponent *>(this_);
    c->remove_object();
}
void RenderComponent::update_material(void *this_, luisa::vector<rbc::RC<rbc::RCBase>> const &mat_vector) {
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object(
        luisa::span{
            reinterpret_cast<RC<world::MaterialResource> const *>(mat_vector.data()),
            mat_vector.size()});
}
void RenderComponent::update_mesh(void *this_, void *mesh) {
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object({}, static_cast<world::MeshResource *>(mesh));
}
void RenderComponent::update_object(void *this_, luisa::vector<rbc::RC<rbc::RCBase>> const &mat_vector, void *mesh) {
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object(
        luisa::span{
            reinterpret_cast<RC<world::MaterialResource> const *>(mat_vector.data()),
            mat_vector.size()},
        static_cast<world::MeshResource *>(mesh));
}
}// namespace rbc