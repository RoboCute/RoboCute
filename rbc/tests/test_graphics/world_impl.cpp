#include "generated/world.h"
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light_component.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_graphics/texture/texture_loader.h>
#include <rbc_world/resources/material.h>
#include <rbc_graphics/graphics_utils.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_world/importers/texture_importer_exr.h>
#include <rbc_world/importers/texture_importer_stb.h>
#include <rbc_core/runtime_static.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_project/project_plugin.h>
#include <rbc_project/project.h>
#include <rbc_world/resources/scene.h>
#include <rbc_core/utils/forget.h>
namespace rbc {
struct EntitiesCollectionImpl : RCBase {
    luisa::vector<RCWeak<world::Entity>> _entities;
};
vstd::Guid Object::guid(void *this_) {
    return static_cast<world::BaseObject *>(this_)->guid();
}
vstd::Guid Object::type_id(void *this_) {
    return static_cast<world::BaseObject *>(this_)->type_id();
}
luisa::string Object::type_name(void *this_) {
    return static_cast<world::BaseObject *>(this_)->type_name();
}
BaseObjectType Object::base_type(void *this_) {
    return static_cast<BaseObjectType>(static_cast<world::BaseObject *>(this_)->base_type());
}
rbc::ResourceLoadStatus Resource::load_status(void *this_) {
    return static_cast<rbc::ResourceLoadStatus>(static_cast<world::Resource *>(this_)->loading_status());
}
void Resource::wait_loading(void *this_) {
    static_cast<world::Resource *>(this_)->wait_loading();
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
    auto comp = e->_create_component(md5);
    if (!comp) {
        return nullptr;
    }
    e->_add_component(comp);
    return comp;
}
void *Entity::get_component(void *this_, luisa::string_view name) {
    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    auto comp = e->get_component(md5);
    return comp;
}
bool Entity::remove_component(void *this_, luisa::string_view name) {
    auto e = static_cast<world::Entity *>(this_);
    luisa::string class_name{"rbc::world::"};
    class_name += name;
    vstd::MD5 md5{luisa::string_view{class_name}};
    return e->remove_component(md5);
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
void TextureResource::create_empty(void *this_, rbc::LCPixelStorage pixel_storage, luisa::uint2 size, uint32_t mip_level, bool is_virtual_texture) {
    auto c = static_cast<world::TextureResource *>(this_);
    c->create_empty(pixel_storage, size, mip_level, is_virtual_texture);
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
bool Resource::install(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    return c->install();
}
void Resource::load(void *this_) {
    auto c = static_cast<world::TextureResource *>(this_);
    c->load();
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
        data->push_back_uninitialized(c->desire_size_bytes());
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

void *Project::_create_() {
    auto ptr = new ProjectImpl();
    manually_add_ref(ptr);
    return ptr;
}
void Project::scan_project(void *this_) {
    auto c = static_cast<ProjectImpl *>(this_);
    if (!c->proj) [[unlikely]] {
        LUISA_ERROR("Project not initialized.");
    }
    c->sync();
    c->proj->scan_project();
}
void Project::init(void *this_, luisa::string_view assets_root_dir) {
    auto c = static_cast<ProjectImpl *>(this_);
    if (c->module || c->proj) return;
    c->module = PluginManager::instance().load_module("rbc_project_plugin");
    c->proj = luisa::unique_ptr<rbc::IProject>(c->module->invoke<ProjectPlugin *()>(
                                                            "get_project_plugin")
                                                   ->create_project(assets_root_dir));
}
// TODO: register guid
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
void Project::import_material(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
    project_import<world::MaterialResource>(this_, path, extra_meta);
}
void Project::import_mesh(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
    project_import<world::MeshResource>(this_, path, extra_meta);
}
void Project::import_texture(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
    project_import<world::TextureResource>(this_, path, extra_meta);
}
void *Project::import_scene(void *this_, luisa::string_view path, luisa::string_view extra_meta) {
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
void *Project::load_resource(void *this_, vstd::Guid const &guid, bool async_load) {
    auto c = static_cast<ProjectImpl *>(this_);
    c->sync();
    auto res = world::load_resource(guid, async_load);
    if (!res) {
        return nullptr;
    }
    auto ptr = res.get();
    rbc::unsafe_forget(std::move(res));
    return ptr;
}

void TextureResource::set_skybox(void *this_) {
    auto t = static_cast<world::TextureResource *>(this_);
    if (t->loading_status() == world::EResourceLoadingStatus::Unloaded) [[unlikely]] {
        LUISA_ERROR("Skybox dest texture not loaded.");
    }
    auto wait_skybox = [&]() -> rbc::coroutine {
        co_await t->await_loading();
    }();
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
    auto c = static_cast<world::SceneResource *>(this_);
    auto entities = c->get_entities(name);
    auto e = static_cast<EntitiesCollectionImpl *>(EntitiesCollection::_create_());
    vstd::push_back_func(e->_entities, entities.size(), [&](size_t i) {
        return entities[i];
    });
    return e;
}
void *Scene::get_entity_by_name(void *this_, luisa::string_view name) {
    auto c = static_cast<world::SceneResource *>(this_);
    auto ptr = c->get_entity(name);
    manually_add_ref(ptr);
    return ptr;
}
void Scene::remove_entity(void *this_, vstd::Guid const &guid) {
    auto c = static_cast<world::SceneResource *>(this_);
    c->remove_entity(guid);
}
void *Scene::get_entity(void *this_, vstd::Guid const &guid) {
    auto c = static_cast<world::SceneResource *>(this_);
    auto ptr = c->get_entity(guid);
    manually_add_ref(ptr);
    return ptr;
}
void *Scene::get_or_add_entity(void *this_, vstd::Guid const &guid) {
    auto c = static_cast<world::SceneResource *>(this_);
    auto ptr = c->get_or_add_entity(guid);
    manually_add_ref(ptr);
    return ptr;
}
void Scene::update_data(void *this_) {
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
    return static_cast<FileMetaImpl *>(this_)->guid;
}
luisa::string FileMeta::meta_json(void *this_) {
    return static_cast<FileMetaImpl *>(this_)->meta_info;
}
void *Project::get_file_meta(void *this_, vstd::Guid const &type_id, luisa::string_view dest_path) {
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
    auto c = static_cast<EntitiesCollectionImpl *>(this_);
    return c->_entities.size();
}
void *EntitiesCollection::get_entity(void *this_, uint64_t index) {
    auto c = static_cast<EntitiesCollectionImpl *>(this_);
    LUISA_ASSERT(index < c->_entities.size(), "Index {} out or range {}", index, c->_entities.size());
    auto v = c->_entities[index].lock().rc();
    if (!v) return nullptr;
    auto ptr = v.get();
    unsafe_forget(std::move(v));
    return ptr;
}
}// namespace rbc