#include <rbc_world_v2/renderer.h>
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/resource_loader.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_world_v2/material.h>
#include <rbc_world_v2/mesh.h>
#include <rbc_graphics/render_device.h>
#include <rbc_world_v2/entity.h>

namespace rbc::world {
void Renderer::_on_transform_update() {
    if (_mesh_tlas_idx != ~0u) {
        auto tr = entity()->get_component<Transform>();
        if (!tr) return;
        _update_object_pos(tr->trs_float());
    }
}
void Renderer::on_awake() {
    add_event(WorldEventType::OnTransformUpdate, &Renderer::_on_transform_update);
}
void Renderer::on_destroy() {
    remove_object();
}
void Renderer::serialize(ObjSerialize const &ser) const {
    auto &ser_obj = ser.ser;
    ser_obj.start_array();
    for (auto &i : _materials) {
        auto guid = i->guid();
        if (guid) {
            ser_obj._store(guid);
            // TODO: mark dependencies
            // ser.depended_resources.emplace(guid);
        }
    }
    ser_obj.add_last_scope_to_object("mats");
    if (_mesh_ref) {
        auto guid = _mesh_ref->guid();
        if (guid) {
            ser_obj._store(guid, "mesh");
            // TODO: mark dependencies
            // ser.depended_resources.emplace(guid);
            // ser.depended_resources.emplace(guid);
        }
    }
}
void Renderer::deserialize(ObjDeSerialize const &ser) {
    auto &obj = ser.ser;
    uint64_t size;
    if (obj.start_array(size, "mats")) {
        _materials.reserve(size);
        for (auto &i : vstd::range(size)) {
            vstd::Guid guid;
            if (!obj._load(guid)) {
                _materials.emplace_back(nullptr);// TODO: deal with empty material
            } else {
                auto obj = load_resource(guid, true);
                if (obj && obj->is_type_of(TypeInfo::get<Material>()))
                    _materials.emplace_back(static_cast<RC<Material> &&>(obj));
            }
        }
        obj.end_scope();
    }
    vstd::Guid guid;
    if (obj._load(guid, "mesh")) {
        auto obj = load_resource(guid, true);
        if (obj && obj->is_type_of(TypeInfo::get<Mesh>())) {
            _mesh_ref = static_cast<RC<Mesh> &&>(obj.get());
        }
    }
}

static bool material_is_emission(luisa::span<RC<Material> const> materials) {
    bool contained_emission = false;
    for (auto &i : materials) {
        i->mat_data().visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, material::OpenPBR>) {
                for (auto &i : t.emission.luminance) {
                    if (i > 1e-3f) {
                        contained_emission = true;
                        return;
                    }
                }
            } else {
                for (auto &i : t.color) {
                    if (i > 1e-3f) {
                        contained_emission = true;
                        return;
                    }
                }
            }
        });
    }
    return contained_emission;
};
Renderer::~Renderer() {
    remove_object();
}
void Renderer::remove_object() {
    auto sm = SceneManager::instance_ptr();
    if (_mesh_ref && _mesh_ref->device_mesh()) {
        _mesh_ref.reset();
    }
    auto dsp = vstd::scope_exit([&]() {
        _mesh_tlas_idx = ~0u;
    });
    if (!sm || _mesh_tlas_idx == ~0u) return;
    switch (_type) {
        case ObjectRenderType::Mesh:
            sm->accel_manager().remove_mesh_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                _mesh_tlas_idx);
            break;
        case ObjectRenderType::EmissionMesh:
            if (Lights::instance()) {
                Lights::instance()->remove_mesh_light(_mesh_light_idx);
            }
            break;
        case ObjectRenderType::Procedural:
            sm->accel_manager().remove_procedural_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                sm->dispose_queue(),
                _procedural_idx);
            break;
    }
}

Renderer::Renderer(Entity *entity) : ComponentDerive<Renderer>(entity) {
    _mesh_light_idx = ~0u;
}
void Renderer::update_object(luisa::span<RC<Material> const> mats, Mesh *mesh) {
    auto tr = entity()->get_component<Transform>();
    if (!tr) return;
    float4x4 matrix = tr->trs_float();
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    if (mesh) {
        if (mesh->empty()) [[unlikely]] {
            LUISA_WARNING("Mesh not loaded, renderer update failed.");
            return;
        }
        _mesh_ref.reset();
        _mesh_ref = mesh;
    } else {
        mesh = _mesh_ref.get();
    }
    if (!mesh) return;
    mesh->wait_load_finished();
    if (!mats.empty()) {
        auto submesh_size = std::max<size_t>(1, mesh->submesh_offsets().size());
        if (!(mats.size() == submesh_size)) [[unlikely]] {
            LUISA_WARNING("Submesh size {} mismatch with material size {}", submesh_size, mats.size());
            return;
        }
        for (auto &i : mats) {
            if (i->empty()) [[unlikely]] {
                LUISA_WARNING("Material not loaded, renderer update failed.");
                return;
            }
        }
        _materials.clear();
        vstd::push_back_all(
            _materials,
            mats);
    }
    if (_materials.size() != mesh->submesh_count()) return;
    _material_codes.clear();
    vstd::push_back_func(
        _material_codes,
        _materials.size(),
        [&](size_t i) {
            auto&& mat = _materials[i];
            mat->init_device_resource();
            return mat->mat_code();
        });
    // TODO: change light type
    bool is_emission = material_is_emission(_materials);
    if (!render_device) return;
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Renderer::update_object can only be called in render-thread.");
    }
    // update
    if (_mesh_tlas_idx != ~0u) {
        switch (_type) {
            case ObjectRenderType::Mesh:
                if (is_emission) {
                    sm.accel_manager().remove_mesh_instance(
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        _mesh_tlas_idx);
                    _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        mesh->device_mesh(),
                        matrix,
                        _material_codes);
                    _type = ObjectRenderType::EmissionMesh;
                } else {
                    sm.accel_manager().set_mesh_instance(
                        _mesh_tlas_idx,
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh->device_mesh()->mesh_data(),
                        _material_codes,
                        matrix);
                }
                break;
            case ObjectRenderType::EmissionMesh:
                if (!is_emission) {
                    Lights::instance()->remove_mesh_light(_mesh_light_idx);
                    _mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh->device_mesh()->mesh_data(),
                        _material_codes,
                        matrix);
                    _type = ObjectRenderType::Mesh;
                } else {
                    Lights::instance()->update_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        _mesh_light_idx,
                        matrix,
                        _material_codes,
                        &mesh->device_mesh());
                }
                break;
            case ObjectRenderType::Procedural: {
                LUISA_ERROR("Procedural not supported.");
            } break;
        }
    }
    // create
    else {
        if (is_emission) {
            _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                mesh->device_mesh(),
                matrix,
                _material_codes);
            _type = ObjectRenderType::EmissionMesh;
        } else {
            _mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                render_device->lc_main_cmd_list(),
                sm.host_upload_buffer(),
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue(),
                mesh->device_mesh()->mesh_data(),
                _material_codes,
                matrix);
            _type = ObjectRenderType::Mesh;
        }
    }
}
void Renderer::_update_object_pos(float4x4 matrix) {
    if (_mesh_tlas_idx == ~0u) [[unlikely]] {
        LUISA_ERROR("Object not initialized yet.");
    }
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    if (!render_device) return;
    switch (_type) {
        case ObjectRenderType::Mesh: {
            sm.accel_manager().set_mesh_instance(
                render_device->lc_main_cmd_list(),
                sm.buffer_uploader(),
                _mesh_tlas_idx,
                matrix, 0xff, true);
        } break;
        case ObjectRenderType::EmissionMesh:
            Lights::instance()->update_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                _mesh_light_idx,
                matrix,
                _material_codes);
            break;
        case ObjectRenderType::Procedural:
            sm.accel_manager().set_procedural_instance(
                _procedural_idx,
                matrix,
                0xffu,
                true);
            break;
    }
}

uint Renderer::get_tlas_index() const {
    switch (_type) {
        case ObjectRenderType::Mesh:
            return _mesh_tlas_idx;
        case ObjectRenderType::EmissionMesh:
            return Lights::instance()->mesh_lights.light_data[_mesh_light_idx].tlas_id;
        case ObjectRenderType::Procedural:
            return _procedural_idx;
        default:
            return ~0u;
    }
}

DECLARE_WORLD_COMPONENT_REGISTER(Renderer);
}// namespace rbc::world