#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4273)// Disable 'inconsistent dll linkage' warning
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdll-attribute-on-redeclaration"
#endif
#include "generated/world.h"
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light_component.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/buffer.h>
#include <rbc_world/components/camera_component.h>
#include <rbc_world/components/data_component.h>
#include <rbc_graphics/graphics_utils.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_core/runtime_static.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_transforming_mesh.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_project/project_plugin.h>
#include <rbc_project/project.h>
#include <rbc_world/resources/scene.h>
#include <rbc_core/utils/forget.h>
#include <rbc_core/state_map.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/render_device.h>
#include <rbc_world/resources/anim_graph.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_world/resources/skin.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/resources/skelmesh.h>
#include <rbc_world/components/skelmesh_component.h>
#include <rbc_world/components/atmosphere_component.h>
#include <rbc_world/resources/gaussian_splat.h>
#include <rbc_world/resources/aabb_voxel.h>
#include <rbc_world/resources/voxel_sdf.h>

#include <rbc_anim/graph/AnimNode_Root.h>
#include <rbc_anim/graph/AnimNode_SequencePlayer.h>

void save_image(luisa::filesystem::path const &path, luisa::compute::Image<float> const &img);// implemented save_image.cpp
namespace rbc {
struct EntitiesCollectionImpl : RCBase {
    luisa::vector<RC<world::Entity>> _entities;
};
vstd::Guid Object::guid(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Object::guid: this_ is null.");
        return {};
    }
    return static_cast<world::BaseObject *>(this_)->guid();
}
vstd::Guid Object::type_id(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Object::type_id: this_ is null.");
        return {};
    }
    return static_cast<world::BaseObject *>(this_)->type_id();
}
bool Object::is_type(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Object::is_type: this_ is null.");
        return false;
    }
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    return static_cast<world::BaseObject *>(this_)->type_id() == md5;
}
luisa::string Object::type_name(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Object::type_name: this_ is null.");
        return {};
    }
    return static_cast<world::BaseObject *>(this_)->type_name();
}
BaseObjectType Object::base_type(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Object::base_type: this_ is null.");
        return BaseObjectType::NONE;
    }
    return static_cast<BaseObjectType>(static_cast<world::BaseObject *>(this_)->base_type());
}
rbc::ResourceLoadStatus Resource::load_status(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::load_status: this_ is null.");
        return static_cast<rbc::ResourceLoadStatus>(0);
    }
    return static_cast<rbc::ResourceLoadStatus>(static_cast<world::Resource *>(this_)->loading_status());
}
void Resource::wait_loading(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::wait_loading: this_ is null.");
        return;
    }
    static_cast<world::Resource *>(this_)->wait_loading();
}
luisa::string Resource::path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::path: this_ is null.");
        return {};
    }
    return luisa::to_string(static_cast<world::Resource *>(this_)->path());
}
bool Resource::save_to_path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::save_to_path: this_ is null.");
        return false;
    }
    return static_cast<world::Resource *>(this_)->save_to_path();
}
void *Entity::add_component(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::add_component: this_ is null.");
        return nullptr;
    }
    if (name.empty()) [[unlikely]] {
        LUISA_ERROR("Entity::add_component: name is empty.");
        return nullptr;
    }
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    auto e = static_cast<world::Entity *>(this_);
    return e->_get_or_add_component(md5, [&]() {
        auto comp = world::Entity::_create_component(md5);
        if (!comp) [[unlikely]] {
            LUISA_ERROR("Try create type {} failed.", class_name);
        }
        return rbc::RC<world::Component>(comp);
    });
}
void *Entity::get_component(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::get_component: this_ is null.");
        return nullptr;
    }
    if (name.empty()) [[unlikely]] {
        LUISA_ERROR("Entity::get_component: name is empty.");
        return nullptr;
    }
    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    auto comp = e->get_component(md5);
    return comp;
}
bool Entity::remove_component(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::remove_component: this_ is null.");
        return false;
    }
    if (name.empty()) [[unlikely]] {
        LUISA_ERROR("Entity::remove_component: name is empty.");
        return false;
    }

    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    return e->remove_component(md5);
}
void *Component::entity(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Component::entity: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::Component *>(this_);
    return c->entity();
}
void Component::update_data(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Component::update_data: this_ is null.");
        return;
    }
    auto c = static_cast<world::Component *>(this_);
    c->update_data();
}
void Component::dispose(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Component::dispose: this_ is null.");
        return;
    }
    auto c = static_cast<world::Component *>(this_);
    c->remove_self_from_entity();
}
uint64_t TransformComponent::children_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::children_count: this_ is null.");
        return 0;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->children().size();
}
luisa::double3 TransformComponent::position(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::position: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->position();
}
bool TransformComponent::remove_children(void *this_, void *children) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::remove_children: this_ is null.");
        return false;
    }
    if (!children) [[unlikely]] {
        LUISA_ERROR("TransformComponent::remove_children: children is null.");
        return false;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->remove_children(static_cast<world::TransformComponent *>(children));
}
luisa::float4 TransformComponent::rotation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::rotation: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->rotation().v;
}
luisa::double3 TransformComponent::scale(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::scale: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->scale();
}
void TransformComponent::set_pos(void *this_, luisa::double3 pos, bool recursive) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::set_pos: this_ is null.");
        return;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_pos(pos, recursive);
}
void TransformComponent::set_rotation(void *this_, luisa::float4 rotation, bool recursive) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::set_rotation: this_ is null.");
        return;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_rotation(rotation, recursive);
}
void TransformComponent::set_scale(void *this_, luisa::double3 scale, bool recursive) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::set_scale: this_ is null.");
        return;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_scale(scale, recursive);
}
void TransformComponent::set_trs(void *this_, luisa::double3 pos, luisa::float4 rotation, luisa::double3 scale, bool recursive) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::set_trs: this_ is null.");
        return;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_trs(
        pos,
        Quaternion{rotation},
        scale,
        recursive);
}
void TransformComponent::set_trs_matrix(void *this_, luisa::double4x4 trs, bool recursive) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::set_trs_matrix: this_ is null.");
        return;
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    c->set_trs(trs, recursive);
}
luisa::double4x4 TransformComponent::trs(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::trs: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->trs();
}
luisa::float4x4 TransformComponent::trs_float(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TransformComponent::trs_float: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TransformComponent *>(this_);
    return c->trs_float();
}
void LightComponent::add_area_light(void *this_, luisa::float3 luminance, bool visible) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::add_area_light: this_ is null.");
        return;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_area_light(luminance, visible);
}
void LightComponent::add_disk_light(void *this_, luisa::float3 luminance, bool visible) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::add_disk_light: this_ is null.");
        return;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_disk_light(luminance, visible);
}
void LightComponent::add_point_light(void *this_, luisa::float3 luminance, bool visible) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::add_point_light: this_ is null.");
        return;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_point_light(luminance, visible);
}
void LightComponent::add_spot_light(void *this_, luisa::float3 luminance, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::add_spot_light: this_ is null.");
        return;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    c->add_spot_light(luminance, angle_radians, small_angle_radians, angle_atten_pow, visible);
}
float LightComponent::angle_atten_pow(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::angle_atten_pow: this_ is null.");
        return 0.0f;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    return c->angle_atten_pow();
}
float LightComponent::angle_radians(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::angle_radians: this_ is null.");
        return 0.0f;
    }
    auto c = static_cast<world::LightComponent *>(this_);
    return c->angle_radians();
}
luisa::float3 LightComponent::luminance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::luminance: this_ is null.");
        return {};
    }
    auto c = static_cast<world::LightComponent *>(this_);
    return c->luminance();
}
float LightComponent::small_angle_radians(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("LightComponent::small_angle_radians: this_ is null.");
        return 0.0f;
    }
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
void TextureResource::create_empty(void *this_, rbc::LCPixelStorage pixel_storage, luisa::uint2 size, uint32_t mip_level, bool is_virtual_texture) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::create_empty: this_ is null.");
        return;
    }
    if (is_virtual_texture) {
        auto row_size = luisa::compute::pixel_storage_size((luisa::compute::PixelStorage)pixel_storage, make_uint3(size.x, is_block_compressed(static_cast<PixelStorage>(pixel_storage)) ? 4u : 1u, 1u));
        if ((row_size & 65535) != 0) [[unlikely]] {
            LUISA_ERROR("TextureResource::create_empty: virtual texture pixel storage size ({}) must be aligned to 65536 bytes.", row_size);
            return;
        }
    }
    auto c = static_cast<world::TextureResource *>(this_);
    c->create_empty(pixel_storage, size, mip_level, is_virtual_texture);
}
luisa::span<std::byte> TextureResource::data_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::data_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TextureResource *>(this_);
    auto ptr = c->host_data();
    if (!ptr) return {};
    if (ptr->empty()) {
        ptr->push_back_uninitialized(c->desire_size_bytes());
    }
    return *ptr;
}
bool TextureResource::has_data_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::has_data_buffer: this_ is null.");
        return false;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->host_data();
}
uint32_t TextureResource::heap_index(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::heap_index: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->heap_index();
}
bool Resource::install(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::install: this_ is null.");
        return false;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->install();
}
void Resource::load(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Resource::load: this_ is null.");
        return;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    c->load();
}
bool TextureResource::is_vt(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::is_vt: this_ is null.");
        return false;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->is_vt();
}
bool TextureResource::load_executed(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::load_executed: this_ is null.");
        return false;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->load_executed();
}
uint32_t TextureResource::mip_level(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::mip_level: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->mip_level();
}
bool TextureResource::pack_to_tile(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::pack_to_tile: this_ is null.");
        return false;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->pack_to_tile();
}
rbc::LCPixelStorage TextureResource::pixel_storage(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::pixel_storage: this_ is null.");
        return static_cast<rbc::LCPixelStorage>(0);
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->pixel_storage();
}
luisa::uint2 TextureResource::size(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::size: this_ is null.");
        return {};
    }
    auto c = static_cast<world::TextureResource *>(this_);
    return c->size();
}
luisa::compute::TextureCreationInfo TextureResource::device_texture(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::device_texture: this_ is null.");
        luisa::compute::TextureCreationInfo r;
        r.invalidate();
        return r;
    }
    auto c = static_cast<world::TextureResource *>(this_);
    luisa::compute::TextureCreationInfo r;
    r.invalidate();
    auto tex = c->tex();
    if (!tex) [[unlikely]] {
        LUISA_ERROR("Texture uninitialized.");
        return r;
    }
    if (!c->is_vt()) [[likely]] {
        auto t = static_cast<DeviceImage *>(tex);
        auto &&img = t->get_float_image();
        if (!img) [[unlikely]] {
            LUISA_ERROR("Texture uninitialized.");
            return r;
        }
        r.format = img.format();
        r.dimension = 2;
        auto s = img.size();
        r.width = s.x;
        r.height = s.y;
        r.depth = 1;
        r.mipmap_levels = img.mip_levels();
        r.handle = img.handle();
        r.native_handle = img.native_handle();
    } else {
        LUISA_ERROR("Sparse image (virtual texture) can not get device_texture.");
    }
    return r;
}

luisa::span<std::byte> MeshResource::pos_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::pos_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    auto vert_count = c->vertex_count();
    auto size_bytes = vert_count * sizeof(luisa::float3);
    if (data->size() < size_bytes) return {};
    return luisa::span{data->data(), size_bytes};
}
luisa::span<std::byte> MeshResource::normal_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::normal_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    if (!c->contained_normal()) return {};
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    auto vert_count = c->vertex_count();
    auto offset = vert_count * sizeof(luisa::float3);
    auto size_bytes = vert_count * sizeof(luisa::float3);
    if (data->size() < offset + size_bytes) return {};
    return luisa::span{data->data() + offset, size_bytes};
}
luisa::span<std::byte> MeshResource::tangent_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::tangent_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    if (!c->contained_tangent()) return {};
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    auto vert_count = c->vertex_count();
    auto offset = vert_count * sizeof(luisa::float3);
    if (c->contained_normal()) {
        offset += vert_count * sizeof(luisa::float3);
    }
    auto size_bytes = vert_count * sizeof(luisa::float4);
    if (data->size() < offset + size_bytes) return {};
    return luisa::span{data->data() + offset, size_bytes};
}
luisa::span<std::byte> MeshResource::uv_buffer(void *this_, uint32_t uv_index) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::uv_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    auto vert_count = c->vertex_count();
    auto uv_count = c->uv_count();
    if (uv_index >= uv_count) [[unlikely]] {
        LUISA_ERROR("UV index {} out or range {}", uv_index, uv_count);
    }

    auto offset = vert_count * sizeof(luisa::float3);
    if (c->contained_normal()) {
        offset += vert_count * sizeof(luisa::float3);
    }
    if (c->contained_tangent()) {
        offset += vert_count * sizeof(luisa::float4);
    }
    offset += uv_index * vert_count * sizeof(luisa::float2);
    auto size_bytes = vert_count * sizeof(luisa::float2);

    if (data->size() < offset + size_bytes) return {};
    return luisa::span{data->data() + offset, size_bytes};
}
luisa::span<std::byte> MeshResource::triangle_indices_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::triangle_indices_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    auto tri_count = c->triangle_count();
    auto tri_size_bytes = tri_count * sizeof(Triangle);
    auto basic_size = c->basic_size_bytes();
    if (data->size() < basic_size) return {};
    auto offset = basic_size - tri_size_bytes;
    return luisa::span{data->data() + offset, tri_size_bytes};
}
uint64_t MeshResource::basic_size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::basic_size_bytes: this_ is null.");
        return 0;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->basic_size_bytes();
}
bool MeshResource::contained_normal(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::contained_normal: this_ is null.");
        return false;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->contained_normal();
}
bool MeshResource::contained_tangent(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::contained_tangent: this_ is null.");
        return false;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->contained_tangent();
}
void MeshResource::create_as_morphing_instance(void *this_, void *origin_mesh) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::create_as_morphing_instance: this_ is null.");
        return;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    c->create_as_morphing_instance(static_cast<world::MeshResource *>(origin_mesh));
}
void MeshResource::create_empty(void *this_, luisa::span<std::byte> offset, uint32_t vertex_count, uint32_t triangle_count, uint32_t uv_count, bool contained_normal, bool contained_tangent) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::create_empty: this_ is null.");
        return;
    }
    luisa::vector<uint> submesh_offsets;
    submesh_offsets.push_back_uninitialized(offset.size_bytes() / sizeof(uint));
    std::memcpy(submesh_offsets.data(), offset.data(), submesh_offsets.size_bytes());
    auto c = static_cast<world::MeshResource *>(this_);
    c->create_empty(std::move(submesh_offsets), vertex_count, triangle_count, uv_count, contained_normal, contained_tangent);
}
luisa::span<std::byte> MeshResource::data_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::data_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    if (data->empty()) [[unlikely]] {
        data->push_back_uninitialized(c->desire_size_bytes());
    }
    return *data;
}
uint64_t MeshResource::desire_size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::desire_size_bytes: this_ is null.");
        return 0;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->desire_size_bytes();
}
uint64_t MeshResource::extra_size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::extra_size_bytes: this_ is null.");
        return 0;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->extra_size_bytes();
}
bool MeshResource::has_data_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::has_data_buffer: this_ is null.");
        return false;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->host_data();
}
bool MeshResource::is_transforming_mesh(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::is_transforming_mesh: this_ is null.");
        return false;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->is_transforming_mesh();
}
uint32_t MeshResource::submesh_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::submesh_count: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->submesh_count();
}
uint32_t MeshResource::triangle_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::triangle_count: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->triangle_count();
}
uint32_t MeshResource::uv_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::uv_count: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->uv_count();
}
uint32_t MeshResource::vertex_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::vertex_count: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    return c->vertex_count();
}
luisa::compute::BufferCreationInfoInterop MeshResource::device_data_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::device_data_buffer: this_ is null.");
        luisa::compute::BufferCreationInfoInterop r;
        r.invalidate();
        return r;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    luisa::compute::BufferCreationInfoInterop r;
    r.invalidate();
    if (c->is_transforming_mesh()) {
        auto mesh = c->device_transforming_mesh();
        if (!mesh) [[unlikely]] {
            LUISA_ERROR("Transforming mesh is null in MeshResource::build_before_tick.");
        }
        if (!mesh->mesh_data()) [[unlikely]] {
            LUISA_ERROR("Mesh data is null in MeshResource::device_data_buffer (transforming mesh).");
        }
        auto &buffer = mesh->mesh_data()->pack.data;
        if (!buffer) [[unlikely]] {
            LUISA_ERROR("Buffer is null in MeshResource::device_data_buffer (transforming mesh).");
        }
        r.handle = buffer.handle();
        r.native_handle = buffer.native_handle();
        r.element_stride = sizeof(uint);
        r.total_size_bytes = buffer.size_bytes();
        r.interop = false;

    } else {
        auto mesh = c->device_mesh();
        if (!mesh) [[unlikely]] {
            LUISA_ERROR("Mesh is null in MeshResource::device_data_buffer.");
        }
        if (!mesh->mesh_data()) [[unlikely]] {
            LUISA_ERROR("Mesh data is null in MeshResource::device_data_buffer.");
        }
        auto &buffer = mesh->mesh_data()->pack.data;
        if (!buffer) [[unlikely]] {
            LUISA_ERROR("Buffer is null in MeshResource::device_data_buffer.");
        }
        r.handle = buffer.handle();
        r.native_handle = buffer.native_handle();
        r.element_stride = sizeof(uint);
        r.total_size_bytes = buffer.size_bytes();
        r.interop = false;
    }
    return r;
}
luisa::compute::BufferCreationInfoInterop MeshResource::device_mutable_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::device_mutable_buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::MeshResource *>(this_);
    luisa::compute::BufferCreationInfoInterop r;
    r.invalidate();
    if (c->is_transforming_mesh()) {
        auto mesh = c->device_transforming_mesh();
        if (!mesh) [[unlikely]] {
            LUISA_ERROR("Transforming mesh is null in MeshResource::build_before_tick.");
        }
        if (!mesh->mesh_data()) [[unlikely]] {
            LUISA_ERROR("Mesh data is null in MeshResource::device_mutable_buffer.");
        }
        auto &buffer = mesh->mesh_data()->pack.mutable_data;
        if (!buffer) [[unlikely]] {
            LUISA_ERROR("Mutable buffer is null in MeshResource::device_mutable_buffer.");
        }
        r.handle = buffer.handle();
        r.native_handle = buffer.native_handle();
        r.element_stride = sizeof(uint);
        r.total_size_bytes = buffer.size_bytes();
        r.interop = false;

    } else {
        LUISA_ERROR("Mutable buffer is only supported for transforming meshes in MeshResource::device_mutable_buffer.");
    }
    return r;
}
void MeshResource::build_before_tick(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MeshResource::build_before_tick: this_ is null.");
        return;
    }
    auto c = static_cast<world::MeshResource *>(this_);
    auto graphics_util = GraphicsUtils::instance();
    if (!graphics_util) [[unlikely]] {
        LUISA_ERROR("GraphicsUtils not initialized.");
    }
    if (c->is_transforming_mesh()) {
        auto mesh = c->device_transforming_mesh();
        if (!mesh) [[unlikely]] {
            LUISA_ERROR("Transforming mesh is null in MeshResource::build_before_tick.");
        }
        graphics_util->build_transforming_mesh(mesh);
    } else {
        auto mesh = c->device_mesh();
        if (!mesh) [[unlikely]] {
            LUISA_ERROR("Mesh is null in MeshResource::build_before_tick.");
        }
        graphics_util->build_mesh(mesh);
    }
}
void MaterialResource::load_from_json(void *this_, luisa::string_view json) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MaterialResource::load_from_json: this_ is null.");
        return;
    }
    auto c = static_cast<world::MaterialResource *>(this_);
    c->load_from_json(json);
}
luisa::string MaterialResource::dump_json(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MaterialResource::dump_json: this_ is null.");
        return luisa::string{};
    }
    auto c = static_cast<world::MaterialResource *>(this_);
    return c->write_content_to_str();
}
uint32_t MaterialResource::mat_code(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("MaterialResource::mat_code: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::MaterialResource *>(this_);
    return c->mat_code().value;
}
void *RenderComponent::get_material(void *this_, uint64_t idx) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::get_material: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    auto materials = c->materials();
    if (idx >= materials.size()) [[unlikely]] {
        LUISA_ERROR("get_material index {} out of range {}", idx, materials.size());
    }
    auto mat = materials[idx].get();
    if (!mat) {
        return nullptr;
    }
    manually_add_ref(mat);
    return mat;
}
uint64_t RenderComponent::mat_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::mat_count: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    return c->materials().size();
}
void *RenderComponent::mesh(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::mesh: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    auto m = c->mesh_ref();
    if (!m) {
        return nullptr;
    }
    manually_add_ref(m);
    return m;
}
uint32_t RenderComponent::get_tlas_index(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::get_tlas_index: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    return c->get_tlas_index();
}
void RenderComponent::remove_object(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::remove_object: this_ is null.");
        return;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    c->remove_object();
}
void RenderComponent::update_material(void *this_, luisa::vector<rbc::RC<rbc::RCBase>> const &mat_vector) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::update_material: this_ is null.");
        return;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object(
        luisa::span{
            reinterpret_cast<RC<world::MaterialResource> const *>(mat_vector.data()),
            mat_vector.size()});
}
void RenderComponent::update_mesh(void *this_, void *mesh) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::update_mesh: this_ is null.");
        return;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object({}, static_cast<world::MeshResource *>(mesh));
}
void RenderComponent::update_object(void *this_, luisa::vector<rbc::RC<rbc::RCBase>> const &mat_vector, void *mesh) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderComponent::update_object: this_ is null.");
        return;
    }
    auto c = static_cast<world::RenderComponent *>(this_);
    c->update_object(
        luisa::span{
            reinterpret_cast<RC<world::MaterialResource> const *>(mat_vector.data()),
            mat_vector.size()},
        static_cast<world::MeshResource *>(mesh));
}
// SkeletonResource implementation
void *SkeletonResource::_create_() {
    auto p = world::create_object<rbc::world::SkeletonResource>();
    manually_add_ref(p);
    return p;
}

void SkeletonResource::log_brief(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::log_brief: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    c->log_brief();
}
int SkeletonResource::get_num_joints(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_num_joints: this_ is null.");
        return -1;
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    return c->ref_skel().num_joints();
}
int SkeletonResource::get_num_soa_joints(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_num_soa_joints: this_ is null.");
        return -1;
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    return c->ref_skel().num_soa_joints();
}
luisa::vector<luisa::string> SkeletonResource::get_joint_names(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_joint_names: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    auto names = c->ref_skel().raw_joint_names();
    luisa::vector<luisa::string> result;
    result.reserve(names.size());
    for (auto &n : names) {
        result.push_back(luisa::string{n});
    }
    return result;
}
luisa::vector<int> SkeletonResource::get_joint_parents(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_joint_parents: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    auto parents = c->ref_skel().raw_joint_parents();
    luisa::vector<int> result;
    result.reserve(parents.size());
    for (auto &p : parents) {
        result.push_back(static_cast<int>(p));
    }
    return result;
}
luisa::vector<luisa::float4x4> SkeletonResource::get_joint_rest_poses(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_joint_rest_poses: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    auto poses = c->ref_skel().joint_rest_poses();

    auto num_joints = c->ref_skel().num_joints();
    luisa::vector<luisa::float4x4> result;
    result.reserve(num_joints);

    for (int i = 0; i < num_joints; ++i) {
        // SOA format: each element contains 4 joints
        int soa_idx = i / 4;
        int soa_offset = i % 4;
        auto &soa = poses[soa_idx];
        // SOA extraction: StorePtrU dumps SimdFloat4 into float[4], then index
        float tx[4], ty[4], tz[4], tw[4];
        ozz::math::StorePtrU(soa.translation.x, tx);
        ozz::math::StorePtrU(soa.translation.y, ty);
        ozz::math::StorePtrU(soa.translation.z, tz);
        luisa::float3 translation(tx[soa_offset], ty[soa_offset], tz[soa_offset]);
        // Extract rotation (quaternion to rotation matrix)
        ozz::math::StorePtrU(soa.rotation.x, tx);
        ozz::math::StorePtrU(soa.rotation.y, ty);
        ozz::math::StorePtrU(soa.rotation.z, tz);
        ozz::math::StorePtrU(soa.rotation.w, tw);
        luisa::float4 quat(tx[soa_offset], ty[soa_offset], tz[soa_offset], tw[soa_offset]);
        float x2 = quat.x + quat.x;
        float y2 = quat.y + quat.y;
        float z2 = quat.z + quat.z;
        float xx = quat.x * x2;
        float xy = quat.x * y2;
        float xz = quat.x * z2;
        float yy = quat.y * y2;
        float yz = quat.y * z2;
        float zz = quat.z * z2;
        float wx = quat.w * x2;
        float wy = quat.w * y2;
        float wz = quat.w * z2;
        luisa::float3x3 rotation_mat(
            luisa::float3(1.0f - (yy + zz), xy + wz, xz - wy),
            luisa::float3(xy - wz, 1.0f - (xx + zz), yz + wx),
            luisa::float3(xz + wy, yz - wx, 1.0f - (xx + yy)));
        // Extract scale (float3)
        ozz::math::StorePtrU(soa.scale.x, tx);
        ozz::math::StorePtrU(soa.scale.y, ty);
        ozz::math::StorePtrU(soa.scale.z, tz);
        luisa::float3 scale(tx[soa_offset], ty[soa_offset], tz[soa_offset]);
        // Build float4x4 transformation matrix (TRS)
        result.push_back(luisa::float4x4(
            luisa::float4(rotation_mat[0][0] * scale.x, rotation_mat[0][1] * scale.y, rotation_mat[0][2] * scale.z, 0.0f),
            luisa::float4(rotation_mat[1][0] * scale.x, rotation_mat[1][1] * scale.y, rotation_mat[1][2] * scale.z, 0.0f),
            luisa::float4(rotation_mat[2][0] * scale.x, rotation_mat[2][1] * scale.y, rotation_mat[2][2] * scale.z, 0.0f),
            luisa::float4(translation.x, translation.y, translation.z, 1.0f)));
    }
    return result;
}
int SkeletonResource::get_parent_index(void *this_, int joint_index) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_parent_index: this_ is null.");
        return -1;
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    return static_cast<int>(c->ref_skel().get_parent_index(joint_index));
}
int SkeletonResource::get_num_bones(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::get_num_bones: this_ is null.");
        return -1;
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    return c->ref_skel().get_num_bones();
}
luisa::vector<int> SkeletonResource::ensure_parents_exist(void *this_, luisa::vector<int> bone_indices) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::ensure_parents_exist: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    luisa::vector<rbc::BoneIndexType> indices;
    indices.reserve(bone_indices.size());
    for (auto idx : bone_indices) {
        indices.push_back(static_cast<rbc::BoneIndexType>(idx));
    }
    c->ref_skel().ensure_parents_exist(indices);
    luisa::vector<int> result;
    result.reserve(indices.size());
    for (auto idx : indices) {
        result.push_back(static_cast<int>(idx));
    }
    return result;
}
luisa::vector<int> SkeletonResource::ensure_parents_exist_and_sort(void *this_, luisa::vector<int> bone_indices) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkeletonResource::ensure_parents_exist_and_sort: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkeletonResource *>(this_);
    luisa::vector<rbc::BoneIndexType> indices;
    indices.reserve(bone_indices.size());
    for (auto idx : bone_indices) {
        indices.push_back(static_cast<rbc::BoneIndexType>(idx));
    }
    c->ref_skel().ensure_parents_exist_and_sort(indices);
    luisa::vector<int> result;
    result.reserve(indices.size());
    for (auto idx : indices) {
        result.push_back(static_cast<int>(idx));
    }
    return result;
}

// SkinResource implementation
void *SkinResource::_create_() {
    auto p = world::create_object<rbc::world::SkinResource>();
    manually_add_ref(p);
    return p;
}
void *SkinResource::ref_skel(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::ref_skel: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    auto skel = c->ref_skel.get();
    if (!skel) return nullptr;
    manually_add_ref(skel);
    return skel;
}
void *SkinResource::ref_mesh(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::ref_mesh: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    auto mesh = c->ref_mesh.get();
    if (!mesh) return nullptr;
    manually_add_ref(mesh);
    return mesh;
}
void SkinResource::generate_LUT(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::generate_LUT: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    c->generate_LUT();
}
void SkinResource::log_brief(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::log_brief: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    c->log_brief();
}
luisa::vector<luisa::string> SkinResource::JointRemaps(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::JointRemaps: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    auto remaps = c->JointRemaps();
    luisa::vector<luisa::string> result;
    result.reserve(remaps.size());
    for (auto &r : remaps) {
        result.push_back(luisa::string{r});
    }
    return result;
}
luisa::vector<luisa::float4x4> SkinResource::InverseBindPoses(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::InverseBindPoses: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    auto poses = c->InverseBindPoses();
    luisa::vector<luisa::float4x4> result;
    result.reserve(poses.size());
    for (auto &p : poses) {
        result.push_back(reinterpret_cast<luisa::float4x4 const &>(p));
    }
    return result;
}
luisa::vector<uint32_t> SkinResource::JointRemapsLUT(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkinResource::JointRemapsLUT: this_ is null.");
        return {};
    }
    auto c = static_cast<rbc::world::SkinResource *>(this_);
    auto lut = c->JointRemapsLUT();
    luisa::vector<uint32_t> result;
    result.reserve(lut.size());
    for (auto &l : lut) {
        result.push_back(static_cast<uint32_t>(l));
    }
    return result;
}
// AnimSequenceResource implementation
void *AnimSequenceResource::_create_() {
    auto p = world::create_object<rbc::world::AnimSequenceResource>();
    manually_add_ref(p);
    return p;
}
void *AnimSequenceResource::ref_skel(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::ref_skel: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    auto skel = c->ref_skel.get();
    if (!skel) return nullptr;
    manually_add_ref(skel);
    return skel;
}
void AnimSequenceResource::log_brief(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::log_brief: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    c->log_brief();
}
luisa::string AnimSequenceResource::get_anim_name(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::get_anim_name: this_ is null.");
        return luisa::string{};
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    return luisa::string{c->anim_name};
}
float AnimSequenceResource::get_sampling_rate(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::get_sampling_rate: this_ is null.");
        return 30.0f;
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    return c->sampling_rate;
}
void AnimSequenceResource::set_anim_name(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::set_anim_name: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    c->anim_name = luisa::string{name};
}
void AnimSequenceResource::set_sampling_rate(void *this_, float rate) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimSequenceResource::set_sampling_rate: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::AnimSequenceResource *>(this_);
    c->sampling_rate = rate;
}

// AnimGraphResource implementation
void *AnimGraphResource::_create_() {
    auto p = world::create_object<rbc::world::AnimGraphResource>();
    manually_add_ref(p);
    return p;
}

bool AnimGraphResource::create_simple_anim_graph(void *this_, void *anim_seq) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AnimGraphResource::create_simple_anim_graph: this_ is null.");
        return false;
    }
    auto c = static_cast<rbc::world::AnimGraphResource *>(this_);
    auto anim_seq_res = static_cast<rbc::world::AnimSequenceResource *>(anim_seq);

    if (!anim_seq_res) {
        return false;
    }

    // Create root node
    auto root = rbc::RC<rbc::AnimNode_Root>::New();
    c->graph().nodes.emplace_back(root);

    // Create sequence player node
    auto seq_player = rbc::RC<rbc::AnimNode_SequencePlayer>::New();
    seq_player->anim_seq_resource = rbc::RC<rbc::world::AnimSequenceResource>{anim_seq_res};
    c->graph().nodes.emplace_back(seq_player);

    // Link root to sequence player
    root->result.linked_node_id = 1;

    c->unsafe_set_loaded();
    return true;
}

// SkelMeshResource implementation
void *SkelMeshResource::_create_() {
    auto p = world::create_object<rbc::world::SkelMeshResource>();
    manually_add_ref(p);
    return p;
}
void *SkelMeshResource::GetSkinResource(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshResource::GetSkinResource: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkelMeshResource *>(this_);
    auto skin = c->get_skin_resource().get();
    if (!skin) return nullptr;
    manually_add_ref(skin);
    return skin;
}
void *SkelMeshResource::ref_skin(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshResource::ref_skin: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkelMeshResource *>(this_);
    auto skin = c->ref_skin.get();
    if (!skin) return nullptr;
    manually_add_ref(skin);
    return skin;
}
void *SkelMeshResource::ref_skeleton(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshResource::ref_skeleton: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkelMeshResource *>(this_);
    auto skel = c->ref_skeleton.get();
    if (!skel) return nullptr;
    manually_add_ref(skel);
    return skel;
}
void *SkelMeshResource::ref_anim_graph(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshResource::ref_anim_graph: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkelMeshResource *>(this_);
    auto graph = c->ref_anim_graph.get();
    if (!graph) return nullptr;
    manually_add_ref(graph);
    return graph;
}

struct ProjectImpl : RCBase {
    luisa::shared_ptr<luisa::DynamicModule> module;
    luisa::unique_ptr<rbc::IProject> proj;
    luisa::fiber::counter counter;
    void sync() {
        counter.wait();
        auto utils = GraphicsUtils::instance();
        if (utils && utils->tex_loader()) utils->tex_loader()->finish_task();
    }
};
void *Project::import_material(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_material: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::MaterialResource>().md5(), luisa::string{});
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::import_mesh(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_mesh: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::MeshResource>().md5(), luisa::string{});
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::import_texture(
    void *this_, luisa::string_view path, uint32_t mip_level, bool to_vt) {
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    // Build meta json for texture import parameters
    luisa::string meta_json = luisa::format(
        "{{\"mip_level\":{},\"is_vt\":{}}}", mip_level, to_vt ? "true" : "false");
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::TextureResource>().md5(), meta_json);
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::import_skeleton(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_skeleton: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::SkeletonResource>().md5(), luisa::string{});
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::import_skin(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_skin: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::SkinResource>().md5(), luisa::string{});
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::import_anim_sequence(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_anim_sequence: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(
        path, TypeInfo::get<world::AnimSequenceResource>().md5(), luisa::string{});
    auto p = ptr.get();
    if (!p) {
        return nullptr;
    }
    ptr->install();
    unsafe_forget(std::move(ptr));
    return p;
}

void *Project::_create_() {
    auto ptr = new ProjectImpl();
    manually_add_ref(ptr);
    return ptr;
}
void Project::scan_project(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::scan_project: this_ is null.");
        return;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    c->proj->scan_project();
}
void Project::init(void *this_, luisa::string_view project_root) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::init: this_ is null.");
        return;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (c->module || c->proj) return;
    c->module = PluginManager::instance().load_module("rbc_project_plugin");
    // project_root：含 rbc_project.json 的项目根目录；
    // 若文件不存在或加载失败则 project_plugin 内部直接报错（fail-first）。
    c->proj = luisa::unique_ptr<rbc::IProject>(c->module->invoke<ProjectPlugin *()>(
                                                            "get_project_plugin")
                                                   ->create_project(project_root));
}

// ===== schema / 路径访问（仅读取 project_plugin 内部状态；字符串跨 DLL 边界）=====
luisa::string Project::root_path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::root_path: this_ is null.");
        return {};
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    return luisa::to_string(c->proj->project_root());
}
luisa::string Project::assets_path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::assets_path: this_ is null.");
        return {};
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    return luisa::to_string(c->proj->assets_dir());
}
luisa::string Project::library_path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::library_path: this_ is null.");
        return {};
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    return luisa::to_string(c->proj->library_dir());
}
luisa::string Project::intermediate_path(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::intermediate_path: this_ is null.");
        return {};
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    return luisa::to_string(c->proj->intermediate_dir());
}
luisa::string Project::config_json(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::config_json: this_ is null.");
        return {};
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    // config_json() 在 project_plugin 内完成序列化，此处不经手 rbc_objser（跨 DLL 约束）
    return c->proj->config_json();
}
template<typename T>
void project_import(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->counter.add();
    luisa::fiber::schedule([counter = c->counter, proj = c->proj.get(), path = luisa::string{path}, extra_meta = luisa::string{extra_meta}] {
        proj->import_assets(path, TypeInfo::get<T>().md5(), extra_meta);
        counter.done();
    });
}
void *Project::import_scene(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::import_scene: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    auto ptr = c->proj->import_assets(path, TypeInfo::get<world::SceneResource>().md5(), luisa::string{extra_meta});
    auto p = ptr.get();
    p->load();
    unsafe_forget(std::move(ptr));
    return p;
}
void *Project::get_resource(void *this_, vstd::Guid const &guid, bool async_load) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::get_resource: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    c->sync();
    auto res = world::get_resource(guid, async_load);
    if (!res) {
        return nullptr;
    }
    auto ptr = res.get();
    rbc::unsafe_forget(std::move(res));
    return ptr;
}

void TextureResource::set_skybox(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("TextureResource::set_skybox: this_ is null.");
        return;
    }
    auto t = static_cast<world::TextureResource *>(this_);
    if (t->loading_status() == world::EResourceLoadingStatus::Unloaded) [[unlikely]] {
        LUISA_ERROR("Skybox dest texture not loaded.");
    }
    auto wait_skybox = rbc::capture([t]() -> rbc::coroutine {
        co_await t->await_loading();
    });
    while (!wait_skybox.done()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        wait_skybox.resume();
    }
    auto graphics = GraphicsUtils::instance();
    if (graphics) {
        t->install();
        graphics->render_plugin()->update_skybox(RC<DeviceImage>{t->get_image()});
    }
}
void *Scene::_create_() {
    auto ptr = world::create_object<world::SceneResource>();
    manually_add_ref(ptr);
    return ptr;
}
void *Scene::get_entities_by_name(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::get_entities_by_name: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    auto entities = c->get_entities(name);
    auto e = static_cast<EntitiesCollectionImpl *>(EntitiesCollection::_create_());
    vstd::push_back_func(e->_entities, entities.size(), [&](size_t i) {
        return entities[i];
    });
    return e;
}
void *Scene::get_entity_by_name(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::get_entity_by_name: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    return c->get_entity(name);
}
void Scene::remove_entity(void *this_, vstd::Guid const &guid) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::remove_entity: this_ is null.");
        return;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    c->remove_entity(guid);
}
void *Scene::get_entity(void *this_, vstd::Guid const &guid) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::get_entity: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    return c->get_entity(guid);
}
void *Scene::get_or_add_entity(void *this_, vstd::Guid const &guid) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::get_or_add_entity: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    return c->get_or_add_entity(guid);
}
void *Scene::add_entity(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::add_entity: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    return c->add_entity();
}
void Scene::update_data(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Scene::update_data: this_ is null.");
        return;
    }
    auto c = static_cast<world::SceneResource *>(this_);
    c->update_data();
}
struct FileMetaImpl : RCBase {
    vstd::Guid guid;
    luisa::string meta_info;
};
void *FileMeta::_create_() {
    auto ptr = new FileMetaImpl{};
    manually_add_ref(ptr);
    return ptr;
}
vstd::Guid FileMeta::guid(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("FileMeta::guid: this_ is null.");
        return vstd::Guid{};
    }
    return static_cast<FileMetaImpl *>(this_)->guid;
}
luisa::string FileMeta::meta_json(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("FileMeta::meta_json: this_ is null.");
        return luisa::string{};
    }
    return static_cast<FileMetaImpl *>(this_)->meta_info;
}
void *Project::get_file_meta(void *this_, vstd::Guid const &type_id, luisa::string_view dest_path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Project::get_file_meta: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    auto meta = static_cast<FileMetaImpl *>(FileMeta::_create_());
    meta->guid.reset();
    luisa::vector<IProject::FileMeta> file_meta;
    c->proj->read_file_metas(
        dest_path,
        file_meta);
    for (auto &i : file_meta) {
        if (i.type_id == type_id) {
            meta->meta_info = i.meta_info;
            meta->guid = i.guid;
            break;
        }
    }
    return meta;
}

void *EntitiesCollection::_create_() {
    auto v = new EntitiesCollectionImpl{};
    manually_add_ref(v);
    return v;
}
uint64_t EntitiesCollection::count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("EntitiesCollection::count: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<EntitiesCollectionImpl *>(this_);
    return c->_entities.size();
}
void *EntitiesCollection::get_entity(void *this_, uint64_t index) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("EntitiesCollection::get_entity: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<EntitiesCollectionImpl *>(this_);
    LUISA_ASSERT(index < c->_entities.size(), "Index {} out or range {}", index, c->_entities.size());
    return c->_entities[index].get();
}
void CameraComponent::set_geometry_export_buffer(void *this_, luisa::compute::BufferCreationInfoInterop buffer, rbc::RendererGeometryType channel_type) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_geometry_export_buffer: this_ is null.");
        return;
    }
    auto cam = static_cast<world::CameraComponent *>(this_);
    auto &map = GraphicsUtils::instance()->render_settings((RenderPlugin::PipeCtxStub *)cam->render_pipe_ctx());
    auto &s = map.read_mut<FrameSettings>();
    s.geometry_channel = (rbc::GeometryType)channel_type;
    s.pt_geometry_buffer =
        (buffer.native_handle == 0 ||
         buffer.handle == invalid_resource_handle) ?
            BufferView<float>{} :
            BufferView<float>(
                buffer.native_handle,
                buffer.handle,
                sizeof(float),
                0,
                buffer.total_size_bytes / sizeof(float),
                buffer.total_size_bytes / sizeof(float));
    RenderDevice::instance().lc_main_stream().synchronize();
}
void CameraComponent::clear_geometry_export_buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::clear_geometry_export_buffer: this_ is null.");
        return;
    }
    auto cam = static_cast<world::CameraComponent *>(this_);
    auto &map = GraphicsUtils::instance()->render_settings((RenderPlugin::PipeCtxStub *)cam->render_pipe_ctx());
    auto &s = map.read_mut<FrameSettings>();
    s.geometry_channel = GeometryType::NONE;
    s.pt_geometry_buffer = {};
}
double CameraComponent::aperture(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::aperture: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->aperture;
}
double CameraComponent::aspect_ratio(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::aspect_ratio: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->aspect_ratio;
}
bool CameraComponent::auto_aspect_ratio(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::auto_aspect_ratio: this_ is null.");
        return false;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->auto_aspect_ratio;
}
void CameraComponent::disable_camera(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::disable_camera: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->disable_camera();
}
void CameraComponent::enable_camera(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::enable_camera: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->enable_camera();
}
bool CameraComponent::enable_physical_camera(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::enable_physical_camera: this_ is null.");
        return false;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->enable_physical_camera;
}
double CameraComponent::far_plane(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::far_plane: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->far_plane;
}
double CameraComponent::focus_distance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::focus_distance: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->focus_distance;
}
double CameraComponent::fov(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::fov: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->fov;
}
double CameraComponent::near_plane(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::near_plane: this_ is null.");
        return 0.0;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    return c->near_plane;
}
void CameraComponent::save_image_to(void *this_, luisa::string_view path) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::save_image_to: this_ is null.");
        return;
    }
    (void)this_;// Suppress unused warning
    auto graphics = GraphicsUtils::instance();
    LUISA_ASSERT(graphics);
    save_image(path, graphics->dst_image());
}
void CameraComponent::set_aperture(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_aperture: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    if (value < 1e-5) {
        LUISA_WARNING("Camera aperture value {} is less than 1e-5, clamping to 1e-5", value);
        value = 1e-5;
    }
    c->aperture = value;
}
void CameraComponent::set_aspect_ratio(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_aspect_ratio: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    if (value < 1e-3) {
        LUISA_WARNING("Camera aspect_ratio value {} is less than 1e-3, clamping to 1e-3", value);
        value = 1e-3;
    }
    c->aspect_ratio = value;
}
void CameraComponent::set_auto_aspect_ratio(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_auto_aspect_ratio: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->auto_aspect_ratio = value;
}
void CameraComponent::set_enable_physical_camera(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_enable_physical_camera: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->enable_physical_camera = value;
}
void CameraComponent::set_far_plane(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_far_plane: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->far_plane = value;
}
void CameraComponent::set_focus_distance(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_focus_distance: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->focus_distance = value;
}
void CameraComponent::set_fov(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_fov: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->fov = value;
}
void CameraComponent::set_near_plane(void *this_, double value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_near_plane: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    c->near_plane = value;
}
void CameraComponent::config_render_image(void *this_, luisa::uint2 size, rbc::LCPixelStorage storage) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::config_render_image: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    if (c->dst_image && all(size == c->dst_image.size()) && (PixelStorage)storage == c->dst_image.storage()) {
        return;
    }
    release_render_image(this_);
    auto rd = RenderDevice::instance_ptr();
    if (!rd) [[unlikely]] {
        LUISA_ERROR("Render device not initialized.");
    }
    c->dst_image = rd->lc_device().create_image<float>((PixelStorage)storage, size);
}
void CameraComponent::release_render_image(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::release_render_image: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    if (!c->dst_image) return;
    auto rd = RenderDevice::instance_ptr();
    if (rd) {
        rd->lc_main_cmd_list().add_callback([i = std::move(c->dst_image)] {});
    } else {
        c->dst_image = {};
    }
}
luisa::compute::TextureCreationInfo CameraComponent::render_image(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::render_image: this_ is null.");
        return {};
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    luisa::compute::TextureCreationInfo r;
    auto &img = c->dst_image;
    if (!img) [[unlikely]] {
        LUISA_ERROR("Camera not enabled or on display calling render_image.");
        r.invalidate();
        return r;
    }
    r.handle = img.handle();
    r.native_handle = img.native_handle();
    r.format = img.format();
    r.dimension = 2;
    r.width = img.size().x;
    r.height = img.size().y;
    r.depth = 1;
    r.mipmap_levels = img.mip_levels();
    return r;
}
void CameraComponent::set_frame_index(void *this_, uint64_t frame_index) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::set_frame_index: this_ is null.");
        return;
    }
    auto c = static_cast<world::CameraComponent *>(this_);
    auto graphics = GraphicsUtils::instance();
    if (c->render_pipe_ctx() && graphics) {
        auto &fs = graphics->render_settings(static_cast<RenderPlugin::PipeCtxStub *>(c->render_pipe_ctx())).read_mut<FrameSettings>();
        fs.frame_index = frame_index;
    }
}

struct BasicDataImpl : RCBase {
    rbc::world::DataComponent::DataType data;
};
void *BasicData::_create_() {
    auto ptr = new BasicDataImpl();
    manually_add_ref(ptr);
    return ptr;
}
template<typename Ret>
Ret basic_type_to_value(void *this_) {
    Ret v{};
    static_cast<BasicDataImpl *>(this_)->data.visit([&]<typename T>(T const &t) {
        if constexpr (requires { v = static_cast<Ret>(t); }) {
            v = static_cast<Ret>(t);
        }
    });
    return v;
}
template<typename Ret>
void basic_type_from_value(void *this_, Ret &&v) {
    static_cast<BasicDataImpl *>(this_)->data.reset_as<std::remove_reference_t<Ret>>(std::forward<Ret>(v));
}
bool BasicData::get_bool(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::get_bool: this_ is null.");
        return false;
    }
    return basic_type_to_value<bool>(this_);
}
double BasicData::get_float(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::get_float: this_ is null.");
        return 0.0;
    }
    return basic_type_to_value<double>(this_);
}
int64_t BasicData::get_int(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::get_int: this_ is null.");
        return -1;
    }
    return basic_type_to_value<int64_t>(this_);
}
luisa::string BasicData::get_string(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::get_string: this_ is null.");
        return luisa::string{};
    }
    return basic_type_to_value<luisa::string>(this_);
}
void BasicData::set_bool(void *this_, bool v) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::set_bool: this_ is null.");
        return;
    }
    basic_type_from_value(this_, v);
}
void BasicData::set_float(void *this_, double v) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::set_float: this_ is null.");
        return;
    }
    basic_type_from_value(this_, v);
}
void BasicData::set_int(void *this_, int64_t v) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::set_int: this_ is null.");
        return;
    }
    basic_type_from_value(this_, v);
}
void BasicData::set_string(void *this_, luisa::string_view v) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::set_string: this_ is null.");
        return;
    }
    basic_type_from_value(this_, luisa::string{v});
}
void *BasicData::get_resource(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::get_resource: this_ is null.");
        return nullptr;
    }
    auto impl = static_cast<BasicDataImpl *>(this_);
    void *res = nullptr;
    impl->data.visit([&]<typename T>(T const &t) {
        if constexpr (std::is_same_v<T, RC<world::Resource>>) {
            res = t.get();
        }
    });
    return res;
}
void BasicData::set_resource(void *this_, void *res) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::set_resource: this_ is null.");
        return;
    }
    auto impl = static_cast<BasicDataImpl *>(this_);
    impl->data.reset_as<RC<world::Resource>>(RC<world::Resource>{static_cast<world::Resource *>(res)});
}
luisa::string Entity::name(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::name: this_ is null.");
        return luisa::string{};
    }
    auto e = static_cast<world::Entity *>(this_);
    return luisa::string{e->name()};
}
void Entity::dispose(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::dispose: this_ is null.");
        return;
    }
    auto e = static_cast<world::Entity *>(this_);
    e->remove_self_from_scene();
}
void Entity::set_name(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("Entity::set_name: this_ is null.");
        return;
    }
    auto e = static_cast<world::Entity *>(this_);
    e->set_name(luisa::string{name});
}
rbc::BasicDataType BasicData::type(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BasicData::type: this_ is null.");
        return {};
    }
    return static_cast<rbc::BasicDataType>(static_cast<BasicDataImpl *>(this_)->data.index());
}

// BufferResource implementation
void *BufferResource::_create_() {
    auto p = world::create_object<world::BufferResource>();
    manually_add_ref(p);
    return p;
}
uint64_t BufferResource::size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BufferResource::size_bytes: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::BufferResource *>(this_);
    return c->size_bytes();
}
luisa::span<std::byte> BufferResource::host_data(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BufferResource::host_data: this_ is null.");
        return {};
    }
    auto c = static_cast<world::BufferResource *>(this_);
    auto data = c->host_data();
    if (!data) return {};
    return *data;
}
luisa::compute::BufferCreationInfoInterop BufferResource::buffer(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BufferResource::buffer: this_ is null.");
        return {};
    }
    auto c = static_cast<world::BufferResource *>(this_);
    luisa::compute::BufferCreationInfoInterop r;
    auto buf = c->buffer();
    if (!buf) {
        r.invalidate();
        return r;
    }
    r.handle = buf.handle();
    r.native_handle = buf.native_handle();
    r.total_size_bytes = buf.size_bytes();
    r.element_stride = 4;
    return r;
}
void BufferResource::create_empty(void *this_, uint64_t size_bytes, bool create_device_buffer) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("BufferResource::create_empty: this_ is null.");
        return;
    }
    auto c = static_cast<world::BufferResource *>(this_);
    c->create_empty(size_bytes, create_device_buffer);
}
// DataComponent implementation
void DataComponent::bind_event(void *this_, rbc::DataComponentEventType event_type, luisa::string_view callback_name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::bind_event: this_ is null.");
        return;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    c->bind_event(static_cast<world::DataComponent::EventType>(event_type), callback_name);
}
void DataComponent::unbind_event(void *this_, rbc::DataComponentEventType event_type) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::unbind_event: this_ is null.");
        return;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    c->unbind_event(static_cast<world::DataComponent::EventType>(event_type));
}
void *DataComponent::get_info(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::get_info: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    auto data = c->get_info(name);
    auto ptr = BasicData::_create_();
    static_cast<BasicDataImpl *>(ptr)->data = std::move(data);
    return ptr;
}
void DataComponent::set_info(void *this_, luisa::string_view name, void *data) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::set_info: this_ is null.");
        return;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    c->set_info(luisa::string{name}, static_cast<BasicDataImpl *>(data)->data);
}
bool DataComponent::has_info(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::has_info: this_ is null.");
        return false;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    return c->has_info(name);
}
bool DataComponent::remove_info(void *this_, luisa::string_view name) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::remove_info: this_ is null.");
        return false;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    return c->remove_info(name);
}
uint64_t DataComponent::info_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::info_count: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    return c->info_count();
}
void DataComponent::clear_infos(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("DataComponent::clear_infos: this_ is null.");
        return;
    }
    auto c = static_cast<world::DataComponent *>(this_);
    c->clear_infos();
}

struct RenderSettingsImpl : RCBase {
    StateMap *map{};
};

// Helper template to clamp values with warning
static void clamp_value_warn(float &value, float min, float max, luisa::string_view name) {
    if (value < min) {
        LUISA_WARNING("RenderSettings: {} value {} is less than {}, clamping to {}", name, value, min, min);
        value = min;
    } else if (value > max) {
        LUISA_WARNING("RenderSettings: {} value {} is greater than {}, clamping to {}", name, value, max, max);
        value = max;
    }
}

static void clamp_value_warn(uint32_t &value, uint32_t min, uint32_t max, luisa::string_view name) {
    if (value < min) {
        LUISA_WARNING("RenderSettings: {} value {} is less than {}, clamping to {}", name, value, min, min);
        value = min;
    } else if (value > max) {
        LUISA_WARNING("RenderSettings: {} value {} is greater than {}, clamping to {}", name, value, max, max);
        value = max;
    }
}

static void clamp_vector2_warn(luisa::float2 &v, float min, float max, luisa::string_view name) {
    if (v.x < min || v.x > max) {
        LUISA_WARNING("RenderSettings: {}.x value {} is out of range [{}, {}], clamping", name, v.x, min, max);
        v.x = std::max(min, std::min(v.x, max));
    }
    if (v.y < min || v.y > max) {
        LUISA_WARNING("RenderSettings: {}.y value {} is out of range [{}, {}], clamping", name, v.y, min, max);
        v.y = std::max(min, std::min(v.y, max));
    }
}

static void clamp_vector3_warn(luisa::float3 &v, float min, float max, luisa::string_view name) {
    if (v.x < min || v.x > max) {
        LUISA_WARNING("RenderSettings: {}.x value {} is out of range [{}, {}], clamping", name, v.x, min, max);
        v.x = std::max(min, std::min(v.x, max));
    }
    if (v.y < min || v.y > max) {
        LUISA_WARNING("RenderSettings: {}.y value {} is out of range [{}, {}], clamping", name, v.y, min, max);
        v.y = std::max(min, std::min(v.y, max));
    }
    if (v.z < min || v.z > max) {
        LUISA_WARNING("RenderSettings: {}.z value {} is out of range [{}, {}], clamping", name, v.z, min, max);
        v.z = std::max(min, std::min(v.z, max));
    }
}

static void clamp_vector4_warn(luisa::float4 &v, float min, float max, luisa::string_view name) {
    if (v.x < min || v.x > max) {
        LUISA_WARNING("RenderSettings: {}.x value {} is out of range [{}, {}], clamping", name, v.x, min, max);
        v.x = std::max(min, std::min(v.x, max));
    }
    if (v.y < min || v.y > max) {
        LUISA_WARNING("RenderSettings: {}.y value {} is out of range [{}, {}], clamping", name, v.y, min, max);
        v.y = std::max(min, std::min(v.y, max));
    }
    if (v.z < min || v.z > max) {
        LUISA_WARNING("RenderSettings: {}.z value {} is out of range [{}, {}], clamping", name, v.z, min, max);
        v.z = std::max(min, std::min(v.z, max));
    }
    if (v.w < min || v.w > max) {
        LUISA_WARNING("RenderSettings: {}.w value {} is out of range [{}, {}], clamping", name, v.w, min, max);
        v.w = std::max(min, std::min(v.w, max));
    }
}

static void clamp_vector3_color_warn(luisa::float3 &v, float min, float max, luisa::string_view name) {
    if (v.x < min || v.x > max) {
        LUISA_WARNING("RenderSettings: {}.x value {} is out of range [{}, {}], clamping", name, v.x, min, max);
        v.x = std::max(min, std::min(v.x, max));
    }
    if (v.y < min || v.y > max) {
        LUISA_WARNING("RenderSettings: {}.y value {} is out of range [{}, {}], clamping", name, v.y, min, max);
        v.y = std::max(min, std::min(v.y, max));
    }
    if (v.z < min || v.z > max) {
        LUISA_WARNING("RenderSettings: {}.z value {} is out of range [{}, {}], clamping", name, v.z, min, max);
        v.z = std::max(min, std::min(v.z, max));
    }
}

static void clamp_vector4_color_warn(luisa::float4 &v, float min_xyz, float max_xyz, float min_w, float max_w, luisa::string_view name) {
    if (v.x < min_xyz || v.x > max_xyz) {
        LUISA_WARNING("RenderSettings: {}.x value {} is out of range [{}, {}], clamping", name, v.x, min_xyz, max_xyz);
        v.x = std::max(min_xyz, std::min(v.x, max_xyz));
    }
    if (v.y < min_xyz || v.y > max_xyz) {
        LUISA_WARNING("RenderSettings: {}.y value {} is out of range [{}, {}], clamping", name, v.y, min_xyz, max_xyz);
        v.y = std::max(min_xyz, std::min(v.y, max_xyz));
    }
    if (v.z < min_xyz || v.z > max_xyz) {
        LUISA_WARNING("RenderSettings: {}.z value {} is out of range [{}, {}], clamping", name, v.z, min_xyz, max_xyz);
        v.z = std::max(min_xyz, std::min(v.z, max_xyz));
    }
    if (v.w < min_w || v.w > max_w) {
        LUISA_WARNING("RenderSettings: {}.w value {} is out of range [{}, {}], clamping", name, v.w, min_w, max_w);
        v.w = std::max(min_w, std::min(v.w, max_w));
    }
}

// ========== SkySettings Getters/Setters ==========
float RenderSettings::get_sky_angle(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sky_angle: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sky_angle : 0.0f;
}
void RenderSettings::set_sky_angle(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sky_angle: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<SkySettings>().sky_angle = value;
}
float RenderSettings::get_sky_max_lum(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sky_max_lum: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sky_max_lum : 65535.0f;
}
void RenderSettings::set_sky_max_lum(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sky_max_lum: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // sky_max_lum: >= 0
    if (value < 0.0f) {
        LUISA_WARNING("RenderSettings: sky_max_lum value {} is less than 0, clamping to 0", value);
        value = 0.0f;
    }
    impl->map->read_mut<SkySettings>().sky_max_lum = value;
}
luisa::float3 RenderSettings::get_sky_color(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sky_color: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sky_color : luisa::float3{1.0f, 1.0f, 1.0f};
}
void RenderSettings::set_sky_color(void *this_, luisa::float3 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sky_color: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // sky_color: 0 ~ 1
    clamp_vector3_warn(value, 0.0f, 1.0f, "sky_color");
    impl->map->read_mut<SkySettings>().sky_color = value;
}
luisa::float3 RenderSettings::get_sun_color(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sun_color: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sun_color : luisa::float3{1.0f, 1.0f, 1.0f};
}
void RenderSettings::set_sun_color(void *this_, luisa::float3 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sun_color: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // sun_color: 0 ~ 1
    clamp_vector3_warn(value, 0.0f, 1.0f, "sun_color");
    impl->map->read_mut<SkySettings>().sun_color = value;
}
luisa::float3 RenderSettings::get_sun_dir(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sun_dir: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sun_dir : luisa::float3{0.0f, -1.0f, 0.0f};
}
void RenderSettings::set_sun_dir(void *this_, luisa::float3 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sun_dir: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // sun_dir: no specific range, typically normalized direction
    impl->map->read_mut<SkySettings>().sun_dir = value;
}
float RenderSettings::get_sun_intensity(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sun_intensity: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sun_intensity : 0.0f;
}
void RenderSettings::set_sun_intensity(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sun_intensity: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // sun_intensity: >= 0
    if (value < 0.0f) {
        LUISA_WARNING("RenderSettings: sun_intensity value {} is less than 0, clamping to 0", value);
        value = 0.0f;
    }
    impl->map->read_mut<SkySettings>().sun_intensity = value;
}
float RenderSettings::get_sun_angle(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_sun_angle: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<SkySettings>();
    return settings ? settings->sun_angle : 0.5f;
}
void RenderSettings::set_sun_angle(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_sun_angle: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<SkySettings>().sun_angle = value;
}

// ========== DisplaySettings Getters/Setters ==========
bool RenderSettings::get_use_linear_sdr(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_use_linear_sdr: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->use_linear_sdr : true;
}
void RenderSettings::set_use_linear_sdr(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_use_linear_sdr: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<DisplaySettings>().use_linear_sdr = value;
}
bool RenderSettings::get_use_hdr_display(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_use_hdr_display: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->use_hdr_display : false;
}
void RenderSettings::set_use_hdr_display(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_use_hdr_display: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<DisplaySettings>().use_hdr_display = value;
}
bool RenderSettings::get_use_hdr_10(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_use_hdr_10: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->use_hdr_10 : false;
}
void RenderSettings::set_use_hdr_10(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_use_hdr_10: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<DisplaySettings>().use_hdr_10 = value;
}
float RenderSettings::get_gamma(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_gamma: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->gamma : 2.2f;
}
void RenderSettings::set_gamma(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_gamma: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // gamma: 0.1f ~ 10.0f
    clamp_value_warn(value, 0.1f, 10.0f, "gamma");
    impl->map->read_mut<DisplaySettings>().gamma = value;
}
float RenderSettings::get_chromatic_aberration(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_chromatic_aberration: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->chromatic_aberration : 0.001f;
}
void RenderSettings::set_chromatic_aberration(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_chromatic_aberration: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // chromatic_aberration: 0 ~ 0.05
    clamp_value_warn(value, 0.0f, 0.05f, "chromatic_aberration");
    impl->map->read_mut<DisplaySettings>().chromatic_aberration = value;
}
rbc::AlphaCull RenderSettings::get_alpha_cull(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_alpha_cull: this_ is null.");
        return rbc::AlphaCull::NoCull;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DisplaySettings>();
    return settings ? settings->alpha_cull : rbc::AlphaCull::NoCull;
}
void RenderSettings::set_alpha_cull(void *this_, rbc::AlphaCull value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_alpha_cull: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<DisplaySettings>().alpha_cull = value;
}

// ========== ExposureSettings Getters/Setters ==========
bool RenderSettings::get_use_auto_exposure(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_use_auto_exposure: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ExposureSettings>();
    return settings ? settings->use_auto_exposure : true;
}
void RenderSettings::set_use_auto_exposure(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_use_auto_exposure: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<ExposureSettings>().use_auto_exposure = value;
}
luisa::float2 RenderSettings::get_filtering(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_filtering: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ExposureSettings>();
    return settings ? settings->filtering : luisa::float2{1.0f, 95.0f};
}
void RenderSettings::set_filtering(void *this_, luisa::float2 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_filtering: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // filtering: 1 ~ 99 (float2 representing min/max percentiles)
    clamp_vector2_warn(value, 1.0f, 99.0f, "filtering");
    // Ensure filtering.x <= filtering.y
    if (value.x > value.y) {
        LUISA_WARNING("RenderSettings: filtering.x ({}) is greater than filtering.y ({}), swapping", value.x, value.y);
        std::swap(value.x, value.y);
    }
    impl->map->read_mut<ExposureSettings>().filtering = value;
}
float RenderSettings::get_min_luminance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_min_luminance: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ExposureSettings>();
    return settings ? settings->minLuminance : -9.0f;
}
void RenderSettings::set_min_luminance(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_min_luminance: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // min_luminance: -9 ~ 8.99
    clamp_value_warn(value, -9.0f, 8.99f, "min_luminance");
    auto &settings = impl->map->read_mut<ExposureSettings>();
    // Ensure min_luminance <= max_luminance
    if (value > settings.maxLuminance) {
        LUISA_WARNING("RenderSettings: min_luminance ({}) is greater than max_luminance ({}), clamping to {}", value, settings.maxLuminance, settings.maxLuminance);
        value = settings.maxLuminance;
    }
    settings.minLuminance = value;
}
float RenderSettings::get_max_luminance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_max_luminance: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ExposureSettings>();
    return settings ? settings->maxLuminance : 9.0f;
}
void RenderSettings::set_max_luminance(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_max_luminance: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // max_luminance: -8.99 ~ 9
    clamp_value_warn(value, -8.99f, 9.0f, "max_luminance");
    auto &settings = impl->map->read_mut<ExposureSettings>();
    // Ensure min_luminance <= max_luminance
    if (value < settings.minLuminance) {
        LUISA_WARNING("RenderSettings: max_luminance ({}) is less than min_luminance ({}), clamping to {}", value, settings.minLuminance, settings.minLuminance);
        value = settings.minLuminance;
    }
    settings.maxLuminance = value;
}
float RenderSettings::get_global_exposure(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_global_exposure: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ExposureSettings>();
    return settings ? settings->globalExposure : 0.5f;
}
void RenderSettings::set_global_exposure(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_global_exposure: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // global_exposure: 1e-3f ~ 256
    clamp_value_warn(value, 1e-3f, 256.0f, "global_exposure");
    impl->map->read_mut<ExposureSettings>().globalExposure = value;
}

// ========== PathTracerSettings Getters/Setters ==========
uint32_t RenderSettings::get_offline_spp(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_offline_spp: this_ is null.");
        return ~0u;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->offline_spp : 1;
}
void RenderSettings::set_offline_spp(void *this_, uint32_t value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_offline_spp: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // offline_spp: 1 ~ 4
    clamp_value_warn(value, 1u, 4u, "offline_spp");
    impl->map->read_mut<PathTracerSettings>().offline_spp = value;
}
uint32_t RenderSettings::get_offline_origin_bounce(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_offline_origin_bounce: this_ is null.");
        return ~0u;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->offline_origin_bounce : 2;
}
void RenderSettings::set_offline_origin_bounce(void *this_, uint32_t value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_offline_origin_bounce: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // offline_origin_bounce: 1 ~ 4
    clamp_value_warn(value, 1u, 4u, "offline_origin_bounce");
    impl->map->read_mut<PathTracerSettings>().offline_origin_bounce = value;
}
uint32_t RenderSettings::get_offline_indirect_bounce(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_offline_indirect_bounce: this_ is null.");
        return ~0u;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->offline_indirect_bounce : 4;
}
void RenderSettings::set_offline_indirect_bounce(void *this_, uint32_t value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_offline_indirect_bounce: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // offline_indirect_bounce: 0 ~ 8
    clamp_value_warn(value, 0u, 8u, "offline_indirect_bounce");
    impl->map->read_mut<PathTracerSettings>().offline_indirect_bounce = value;
}
bool RenderSettings::get_denoise(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_denoise: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->denoise : true;
}
void RenderSettings::set_denoise(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_denoise: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<PathTracerSettings>().denoise = value;
}
bool RenderSettings::get_reject_sampling(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_reject_sampling: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<FrameSettings>();
    return settings ? settings->reject_sampling : false;
}
void RenderSettings::set_reject_sampling(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_reject_sampling: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<FrameSettings>().reject_sampling = value;
}

// ========== AO Settings Getters/Setters ==========
bool RenderSettings::get_enable_ao_mode(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_enable_ao_mode: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->enable_ao_mode : false;
}
void RenderSettings::set_enable_ao_mode(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_enable_ao_mode: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<PathTracerSettings>().enable_ao_mode = value;
}
bool RenderSettings::get_ao_use_cosine_sample(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_ao_use_cosine_sample: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->ao_use_cosine_sample : false;
}
void RenderSettings::set_ao_use_cosine_sample(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_ao_use_cosine_sample: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<PathTracerSettings>().ao_use_cosine_sample = value;
}
float4 RenderSettings::get_ao_max_radius(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_ao_max_radius: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->ao_max_radius : float4(1.0f);
}
void RenderSettings::set_ao_max_radius(void *this_, float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_ao_max_radius: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // ao_max_radius: >= 0
    value = max(value, float4(1e-3f));
    impl->map->read_mut<PathTracerSettings>().ao_max_radius = value;
}
float4 RenderSettings::get_ao_atten_pow(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_ao_atten_pow: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<PathTracerSettings>();
    return settings ? settings->ao_atten_pow : float4(1.0f);
}
void RenderSettings::set_ao_atten_pow(void *this_, float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_ao_atten_pow: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // ao_atten_pow: >= 0.001
    value = max(value, float4(1e-3f));
    impl->map->read_mut<PathTracerSettings>().ao_atten_pow = value;
}

// ========== DistortionSettings Getters/Setters ==========
float RenderSettings::get_distortion_scale(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_distortion_scale: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DistortionSettings>();
    return settings ? settings->scale : 1.0f;
}
void RenderSettings::set_distortion_scale(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_distortion_scale: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // scale: 0.01f ~ 5.0f
    clamp_value_warn(value, 0.01f, 5.0f, "distortion_scale");
    impl->map->read_mut<DistortionSettings>().scale = value;
}
float RenderSettings::get_distortion_intensity(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_distortion_intensity: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DistortionSettings>();
    return settings ? settings->intensity : 0.0f;
}
void RenderSettings::set_distortion_intensity(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_distortion_intensity: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // intensity: -100 ~ 100
    clamp_value_warn(value, -100.0f, 100.0f, "distortion_intensity");
    impl->map->read_mut<DistortionSettings>().intensity = value;
}
luisa::float2 RenderSettings::get_distortion_intensity_multiplier(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_distortion_intensity_multiplier: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DistortionSettings>();
    return settings ? settings->intensity_multiplier : luisa::float2{1.0f, 1.0f};
}
void RenderSettings::set_distortion_intensity_multiplier(void *this_, luisa::float2 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_distortion_intensity_multiplier: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // intensity_multiplier: 0 ~ 1
    clamp_vector2_warn(value, 0.0f, 1.0f, "distortion_intensity_multiplier");
    impl->map->read_mut<DistortionSettings>().intensity_multiplier = value;
}
luisa::float2 RenderSettings::get_distortion_center(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_distortion_center: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<DistortionSettings>();
    return settings ? settings->center : luisa::float2{0.0f, 0.0f};
}
void RenderSettings::set_distortion_center(void *this_, luisa::float2 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_distortion_center: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // center: -1 ~ 1
    clamp_vector2_warn(value, -1.0f, 1.0f, "distortion_center");
    impl->map->read_mut<DistortionSettings>().center = value;
}

// ========== ToneMappingSettings - LPM Getters/Setters ==========
bool RenderSettings::get_lpm_shoulder(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_shoulder: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.shoulder : true;
}
void RenderSettings::set_lpm_shoulder(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_shoulder: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<ToneMappingSettings>().lpm.shoulder = value;
}
float RenderSettings::get_lpm_soft_gap(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_soft_gap: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.softGap : 0.0f;
}
void RenderSettings::set_lpm_soft_gap(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_soft_gap: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // soft_gap: no specific range, but typically >= 0
    if (value < 0.0f) {
        LUISA_WARNING("RenderSettings: lpm_soft_gap value {} is less than 0.001, clamping to 0.001", value);
        value = 0.0f;
    }
    impl->map->read_mut<ToneMappingSettings>().lpm.softGap = value;
}
float RenderSettings::get_lpm_hdr_max(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_hdr_max: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.hdrMax : 1847.0f;
}
void RenderSettings::set_lpm_hdr_max(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_hdr_max: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // hdr_max: > 0
    if (value <= 1e-3) {
        LUISA_WARNING("RenderSettings: lpm_hdr_max value {} is less than or equal to 0.001, clamping to 0.001", value);
        value = 1e-3f;
    }
    impl->map->read_mut<ToneMappingSettings>().lpm.hdrMax = value;
}
float RenderSettings::get_lpm_exposure(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_exposure: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.lpmExposure : 10.0f;
}
void RenderSettings::set_lpm_exposure(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_exposure: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // lpm_exposure: > 0
    if (value <= 1e-3) {
        LUISA_WARNING("RenderSettings: lpm_exposure value {} is less than or equal to 0, clamping to 1e-3", value);
        value = 1e-3f;
    }
    impl->map->read_mut<ToneMappingSettings>().lpm.lpmExposure = value;
}
float RenderSettings::get_lpm_contrast(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_contrast: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.contrast : 0.0f;
}
void RenderSettings::set_lpm_contrast(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_contrast: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // contrast: no specific range, typically [-1, 1]
    clamp_value_warn(value, -1.0f, 1.0f, "lpm_contrast");
    impl->map->read_mut<ToneMappingSettings>().lpm.contrast = value;
}
float RenderSettings::get_lpm_shoulder_contrast(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_shoulder_contrast: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.shoulderContrast : 1.0f;
}
void RenderSettings::set_lpm_shoulder_contrast(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_shoulder_contrast: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // shoulder_contrast: > 0
    if (value <= 1e-3f) {
        LUISA_WARNING("RenderSettings: lpm_shoulder_contrast value {} is less than or equal to 0, clamping to 1e-3", value);
        value = 1e-3f;
    }
    impl->map->read_mut<ToneMappingSettings>().lpm.shoulderContrast = value;
}
luisa::float3 RenderSettings::get_lpm_saturation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_saturation: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.saturation : luisa::float3{0.0f, 0.0f, 0.0f};
}
void RenderSettings::set_lpm_saturation(void *this_, luisa::float3 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_saturation: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // saturation: -1 ~ 1 (vector3)
    clamp_vector3_warn(value, -1.0f, 1.0f, "lpm_saturation");
    impl->map->read_mut<ToneMappingSettings>().lpm.saturation = value;
}
luisa::float3 RenderSettings::get_lpm_crosstalk(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_crosstalk: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.crosstalk : luisa::float3{1.0f, 1.0f, 1.0f};
}
void RenderSettings::set_lpm_crosstalk(void *this_, luisa::float3 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_crosstalk: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // crosstalk: color values 0 ~ 1 (vector3)
    clamp_vector3_warn(value, 0.0f, 1.0f, "lpm_crosstalk");
    impl->map->read_mut<ToneMappingSettings>().lpm.crosstalk = value;
}
float RenderSettings::get_lpm_display_min_luminance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_display_min_luminance: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.displayMinLuminance : 0.001f;
}
void RenderSettings::set_lpm_display_min_luminance(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_display_min_luminance: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // display_min_luminance: >= 0
    if (value < 0.0f) {
        LUISA_WARNING("RenderSettings: lpm_display_min_luminance value {} is less than 0, clamping to 0", value);
        value = 0.0f;
    }
    auto &settings = impl->map->read_mut<ToneMappingSettings>().lpm;
    // Ensure display_min_luminance <= display_max_luminance
    if (value > settings.displayMaxLuminance) {
        LUISA_WARNING("RenderSettings: lpm_display_min_luminance ({}) is greater than display_max_luminance ({}), clamping to {}", value, settings.displayMaxLuminance, settings.displayMaxLuminance);
        value = settings.displayMaxLuminance;
    }
    settings.displayMinLuminance = value;
}
float RenderSettings::get_lpm_display_max_luminance(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_lpm_display_max_luminance: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->lpm.displayMaxLuminance : 1000.0f;
}
void RenderSettings::set_lpm_display_max_luminance(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_lpm_display_max_luminance: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto &settings = impl->map->read_mut<ToneMappingSettings>().lpm;
    // Ensure display_max_luminance >= display_min_luminance
    if (value < settings.displayMinLuminance) {
        LUISA_WARNING("RenderSettings: lpm_display_max_luminance ({}) is less than display_min_luminance ({}), clamping to {}", value, settings.displayMinLuminance, settings.displayMinLuminance);
        value = settings.displayMinLuminance;
    }
    settings.displayMaxLuminance = value;
}

// ========== ToneMappingSettings - ACES Getters/Setters ==========
float RenderSettings::get_aces_temperature(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_temperature: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.temperature : 6500.0f;
}
void RenderSettings::set_aces_temperature(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_temperature: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // temperature: 1000.f ~ 15000.f
    clamp_value_warn(value, 1000.0f, 15000.0f, "aces_temperature");
    impl->map->read_mut<ToneMappingSettings>().aces.temperature = value;
}
float RenderSettings::get_aces_tint(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_tint: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.tint : 0.0f;
}
void RenderSettings::set_aces_tint(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_tint: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // tint: -1 ~ 1
    clamp_value_warn(value, -1.0f, 1.0f, "aces_tint");
    impl->map->read_mut<ToneMappingSettings>().aces.tint = value;
}
bool RenderSettings::get_aces_use_white_balance_mode(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_use_white_balance_mode: this_ is null.");
        return false;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.use_white_balance_mode : false;
}
void RenderSettings::set_aces_use_white_balance_mode(void *this_, bool value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_use_white_balance_mode: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->read_mut<ToneMappingSettings>().aces.use_white_balance_mode = value;
}
float RenderSettings::get_aces_hue_shift(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_hue_shift: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.hueShift : 0.0f;
}
void RenderSettings::set_aces_hue_shift(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_hue_shift: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // hue_shift: -100 ~ 100
    clamp_value_warn(value, -100.0f, 100.0f, "aces_hue_shift");
    impl->map->read_mut<ToneMappingSettings>().aces.hueShift = value;
}
float RenderSettings::get_aces_saturation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_saturation: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.saturation : 0.0f;
}
void RenderSettings::set_aces_saturation(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_saturation: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // saturation: -100 ~ 200
    clamp_value_warn(value, -100.0f, 200.0f, "aces_saturation");
    impl->map->read_mut<ToneMappingSettings>().aces.saturation = value;
}
float RenderSettings::get_aces_contrast(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_contrast: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.contrast : 0.0f;
}
void RenderSettings::set_aces_contrast(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_contrast: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // contrast: -100 ~ 100
    clamp_value_warn(value, -100.0f, 100.0f, "aces_contrast");
    impl->map->read_mut<ToneMappingSettings>().aces.contrast = value;
}
float RenderSettings::get_aces_mixer_red_out_red_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_red_out_red_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerRedOutRedIn : 100.0f;
}
void RenderSettings::set_aces_mixer_red_out_red_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_red_out_red_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // Channel mixer values: -200 ~ 200
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_red_out_red_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerRedOutRedIn = value;
}
float RenderSettings::get_aces_mixer_red_out_green_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_red_out_green_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerRedOutGreenIn : 0.0f;
}
void RenderSettings::set_aces_mixer_red_out_green_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_red_out_green_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_red_out_green_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerRedOutGreenIn = value;
}
float RenderSettings::get_aces_mixer_red_out_blue_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_red_out_blue_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerRedOutBlueIn : 0.0f;
}
void RenderSettings::set_aces_mixer_red_out_blue_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_red_out_blue_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_red_out_blue_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerRedOutBlueIn = value;
}
float RenderSettings::get_aces_mixer_green_out_red_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_green_out_red_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerGreenOutRedIn : 0.0f;
}
void RenderSettings::set_aces_mixer_green_out_red_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_green_out_red_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_green_out_red_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerGreenOutRedIn = value;
}
float RenderSettings::get_aces_mixer_green_out_green_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_green_out_green_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerGreenOutGreenIn : 100.0f;
}
void RenderSettings::set_aces_mixer_green_out_green_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_green_out_green_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_green_out_green_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerGreenOutGreenIn = value;
}
float RenderSettings::get_aces_mixer_green_out_blue_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_green_out_blue_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerGreenOutBlueIn : 0.0f;
}
void RenderSettings::set_aces_mixer_green_out_blue_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_green_out_blue_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_green_out_blue_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerGreenOutBlueIn = value;
}
float RenderSettings::get_aces_mixer_blue_out_red_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_blue_out_red_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerBlueOutRedIn : 0.0f;
}
void RenderSettings::set_aces_mixer_blue_out_red_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_blue_out_red_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_blue_out_red_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerBlueOutRedIn = value;
}
float RenderSettings::get_aces_mixer_blue_out_green_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_blue_out_green_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerBlueOutGreenIn : 0.0f;
}
void RenderSettings::set_aces_mixer_blue_out_green_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_blue_out_green_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_blue_out_green_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerBlueOutGreenIn = value;
}
float RenderSettings::get_aces_mixer_blue_out_blue_in(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_mixer_blue_out_blue_in: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.mixerBlueOutBlueIn : 100.0f;
}
void RenderSettings::set_aces_mixer_blue_out_blue_in(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_mixer_blue_out_blue_in: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    clamp_value_warn(value, -200.0f, 200.0f, "aces_mixer_blue_out_blue_in");
    impl->map->read_mut<ToneMappingSettings>().aces.mixerBlueOutBlueIn = value;
}
luisa::float4 RenderSettings::get_aces_lift(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_lift: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.lift : luisa::float4{1.0f, 1.0f, 1.0f, 0.0f};
}
void RenderSettings::set_aces_lift(void *this_, luisa::float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_lift: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // lift: xyz color 0 ~ 1, w: -1 ~ 1
    clamp_vector4_color_warn(value, 0.0f, 1.0f, -1.0f, 1.0f, "aces_lift");
    impl->map->read_mut<ToneMappingSettings>().aces.lift = value;
}
luisa::float4 RenderSettings::get_aces_gamma(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_gamma: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.gamma : luisa::float4{1.0f, 1.0f, 1.0f, 0.0f};
}
void RenderSettings::set_aces_gamma(void *this_, luisa::float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_gamma: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // gamma: xyz color 0 ~ 1, w: -1 ~ 1
    clamp_vector4_color_warn(value, 0.0f, 1.0f, -1.0f, 1.0f, "aces_gamma");
    impl->map->read_mut<ToneMappingSettings>().aces.gamma = value;
}
luisa::float4 RenderSettings::get_aces_gain(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_gain: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.gain : luisa::float4{1.0f, 1.0f, 1.0f, 0.0f};
}
void RenderSettings::set_aces_gain(void *this_, luisa::float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_gain: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // gain: xyz color 0 ~ 1, w: -1 ~ 1
    clamp_vector4_color_warn(value, 0.0f, 1.0f, -1.0f, 1.0f, "aces_gain");
    impl->map->read_mut<ToneMappingSettings>().aces.gain = value;
}
luisa::float4 RenderSettings::get_aces_color_filter(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_color_filter: this_ is null.");
        return {};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.colorFilter : luisa::float4{1.0f, 1.0f, 1.0f, 1.0f};
}
void RenderSettings::set_aces_color_filter(void *this_, luisa::float4 value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_color_filter: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // color_filter: xyz color 0 ~ 1, w: 0 ~ 5
    clamp_vector4_color_warn(value, 0.0f, 1.0f, 0.0f, 5.0f, "aces_color_filter");
    impl->map->read_mut<ToneMappingSettings>().aces.colorFilter = value;
}
float RenderSettings::get_aces_hdr_display_multiplier(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_hdr_display_multiplier: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.tone_mapping.hdr_display_multiplier : 5.0f;
}
void RenderSettings::set_aces_hdr_display_multiplier(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_hdr_display_multiplier: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // hdr_display_multiplier: 1e-3f ~ 100.0f
    clamp_value_warn(value, 1e-3f, 100.0f, "aces_hdr_display_multiplier");
    impl->map->read_mut<ToneMappingSettings>().aces.tone_mapping.hdr_display_multiplier = value;
}
float RenderSettings::get_aces_hdr_paper_white(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::get_aces_hdr_paper_white: this_ is null.");
        return 0.0f;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto settings = impl->map->read_if<ToneMappingSettings>();
    return settings ? settings->aces.tone_mapping.hdr_paper_white : 80.0f;
}
void RenderSettings::set_aces_hdr_paper_white(void *this_, float value) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::set_aces_hdr_paper_white: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    // hdr_paper_white: 80.0f ~ 1000.0f
    clamp_value_warn(value, 80.0f, 1000.0f, "aces_hdr_paper_white");
    impl->map->read_mut<ToneMappingSettings>().aces.tone_mapping.hdr_paper_white = value;
}

luisa::string RenderSettings::serialize_to_json(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::serialize_to_json: this_ is null.");
        return luisa::string{};
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    auto blob = impl->map->serialize_to_json();
    if (!blob.data()) {
        return {};
    }
    return luisa::string{(char const *)blob.data(), blob.size()};
}

void RenderSettings::deserialize_from_json(void *this_, luisa::string_view json) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("RenderSettings::deserialize_from_json: this_ is null.");
        return;
    }
    auto impl = static_cast<RenderSettingsImpl *>(this_);
    LUISA_DEBUG_ASSERT(impl->map, "Map is null");
    impl->map->init_json(json);
}

void *RenderSettings::_create_() {
    auto ptr = new RenderSettingsImpl{};
    manually_add_ref(ptr);
    return ptr;
}

void *CameraComponent::render_settings(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("CameraComponent::render_settings: this_ is null.");
        return nullptr;
    }
    auto cam = static_cast<world::CameraComponent *>(this_);
    auto render_settings = cam->render_pipe_ctx();
    auto graphics = GraphicsUtils::instance();
    if (!graphics) [[unlikely]] {
        LUISA_ERROR("GraphicsUtils not initialized.");
    }
    auto &map = graphics->render_settings((RenderPlugin::PipeCtxStub *)render_settings);
    auto settings = static_cast<RenderSettingsImpl *>(RenderSettings::_create_());
    settings->map = &map;
    return settings;
}
void *AtmosphereComponent::texture(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AtmosphereComponent::texture: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::AtmosphereComponent *>(this_);
    auto tex = c->hdri.get();
    if (!tex) return nullptr;
    manually_add_ref(tex);
    return tex;
}
void AtmosphereComponent::update_texture(void *this_, void *tex) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("AtmosphereComponent::update_texture: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::AtmosphereComponent *>(this_);
    c->hdri = static_cast<rbc::world::TextureResource *>(tex);
}

// SkelMeshComponent implementation
void *SkelMeshComponent::get_runtime_mesh(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::GetRuntimeMesh: this_ is null.");
        return nullptr;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    auto mesh = c->GetRuntimeMesh();
    if (!mesh) return nullptr;
    manually_add_ref(mesh);
    return mesh;
}
void SkelMeshComponent::remove_object(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::remove_object: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->remove_object();
}
void SkelMeshComponent::tick(void *this_, float delta_time) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::tick: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->tick(delta_time);
}
void SkelMeshComponent::update_render(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::update_render: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->update_render();
}
bool SkelMeshComponent::is_enabled(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::is_enabled: this_ is null.");
        return false;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->IsEnabled();
}
void SkelMeshComponent::set_ref_skel_mesh(void *this_, void *skel_mesh) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::set_ref_skel_mesh: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    auto skel_mesh_rc = RC<rbc::world::SkelMeshResource>{static_cast<rbc::world::SkelMeshResource *>(skel_mesh)};
    c->SetRefSkelMesh(skel_mesh_rc);
}
void SkelMeshComponent::play_animation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::play_animation: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->PlayAnimation();
}
void SkelMeshComponent::pause_animation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::pause_animation: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->PauseAnimation();
}
void SkelMeshComponent::stop_animation(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::stop_animation: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->StopAnimation();
}
void SkelMeshComponent::set_animation_time(void *this_, float time) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::set_animation_time: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->time = time;
}
float SkelMeshComponent::get_animation_time(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::get_animation_time: this_ is null.");
        return 0.0f;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->time;
}
void SkelMeshComponent::set_playback_speed(void *this_, float speed) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::set_playback_speed: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->SetPlaybackSpeed(speed);
}
float SkelMeshComponent::get_playback_speed(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::get_playback_speed: this_ is null.");
        return 1.0f;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->GetPlaybackSpeed();
}
bool SkelMeshComponent::is_playing(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::is_playing: this_ is null.");
        return false;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->IsEnabled();
}
luisa::float4x4 SkelMeshComponent::get_bone_transform(void *this_, int bone_index) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::get_bone_transform: this_ is null.");
        return make_float4x4(1.0f);
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->GetBoneTransform(bone_index);
}
void SkelMeshComponent::set_bone_transform(void *this_, int bone_index, luisa::float4x4 transform) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::set_bone_transform: this_ is null.");
        return;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    c->SetBoneTransform(bone_index, transform);
}
int SkelMeshComponent::get_num_bones(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SkelMeshComponent::get_num_bones: this_ is null.");
        return 0;
    }
    auto c = static_cast<rbc::world::SkelMeshComponent *>(this_);
    return c->GetNumBones();
}

// VoxelResource implementation
void *VoxelResource::_create_() {
    auto p = world::create_object<world::VoxelResource>();
    manually_add_ref(p);
    return p;
}
void VoxelResource::create_empty(void *this_, uint32_t num_voxels) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::create_empty: this_ is null.");
        return;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    c->create_empty(num_voxels);
}
bool VoxelResource::empty(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::empty: this_ is null.");
        return false;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    return c->empty();
}
uint64_t VoxelResource::host_data_size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::host_data_size_bytes: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    return c->host_data_size_bytes();
}
bool VoxelResource::is_procedural_dirty(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::is_procedural_dirty: this_ is null.");
        return false;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    return c->is_procedural_dirty();
}
uint32_t VoxelResource::num_voxels(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::num_voxels: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    return c->num_voxels();
}
uint32_t VoxelResource::procedural_instance_id(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("VoxelResource::procedural_instance_id: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::VoxelResource *>(this_);
    return c->procedural_instance_id();
}

// SDFVoxelResource implementation
void *SDFVoxelResource::_create_() {
    auto p = world::create_object<world::SDFVoxelResource>();
    manually_add_ref(p);
    return p;
}
void SDFVoxelResource::create_empty(void *this_, luisa::uint3 grid_size) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::create_empty: this_ is null.");
        return;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    c->create_empty(grid_size);
}
bool SDFVoxelResource::empty(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::empty: this_ is null.");
        return false;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->empty();
}
luisa::uint3 SDFVoxelResource::grid_size(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::grid_size: this_ is null.");
        return {};
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->grid_size();
}
uint64_t SDFVoxelResource::host_data_size_bytes(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::host_data_size_bytes: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->host_data_size_bytes();
}
bool SDFVoxelResource::is_procedural_dirty(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::is_procedural_dirty: this_ is null.");
        return false;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->is_procedural_dirty();
}
uint64_t SDFVoxelResource::num_voxels(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::num_voxels: this_ is null.");
        return ~0ull;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->num_voxels();
}
uint32_t SDFVoxelResource::procedural_instance_id(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::procedural_instance_id: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->procedural_instance_id();
}
uint32_t SDFVoxelResource::sample_count(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::sample_count: this_ is null.");
        return ~0u;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->sample_count();
}
void SDFVoxelResource::set_sample_count(void *this_, uint32_t count) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::set_sample_count: this_ is null.");
        return;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    c->set_sample_count(count);
}
void SDFVoxelResource::set_uvw_offset(void *this_, luisa::float3 offset) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::set_uvw_offset: this_ is null.");
        return;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    c->set_uvw_offset(offset);
}
void SDFVoxelResource::set_uvw_scale(void *this_, luisa::float3 scale) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::set_uvw_scale: this_ is null.");
        return;
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    c->set_uvw_scale(scale);
}
luisa::float3 SDFVoxelResource::uvw_offset(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::uvw_offset: this_ is null.");
        return {};
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->uvw_offset();
}
luisa::float3 SDFVoxelResource::uvw_scale(void *this_) {
    if (!this_) [[unlikely]] {
        LUISA_ERROR("SDFVoxelResource::uvw_scale: this_ is null.");
        return {};
    }
    auto c = static_cast<world::SDFVoxelResource *>(this_);
    return c->uvw_scale();
}
}// namespace rbc
