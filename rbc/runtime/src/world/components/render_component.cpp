#include <rbc_world/components/render_component.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/type_register.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_graphics/render_device.h>
#include <rbc_world/entity.h>
#include <rbc_core/runtime_static.h>
/*
TODO: only should update light bvh while in ray-tracing mode
*/
namespace rbc::world {
struct RenderComponentLists {
    rbc::shared_atomic_mutex _mtx;
    luisa::unordered_map<uint32_t, vstd::Guid> accel_ids;// user_id : RenderComponent
};
static RuntimeStatic<RenderComponentLists> render_comp_lists;
RC<RenderComponent> RenderComponent::try_get_component(uint user_id) {
    std::shared_lock lck{render_comp_lists->_mtx};
    auto iter = render_comp_lists->accel_ids.find(user_id);
    if (iter == render_comp_lists->accel_ids.end()) return {};
    auto obj_ref = get_object_ref(iter->second);
    if (!obj_ref || !obj_ref->is_type_of<RenderComponent>()) return {};
    return std::move(obj_ref).cast_static<RenderComponent>();
}
void RenderComponent::_on_transform_update() {
    if (_mesh_tlas_idx != ~0u) {
        auto tr = entity()->get_component<TransformComponent>();
        if (!tr) return;
        _update_object_pos(tr->trs_float());
    }
}
void RenderComponent::on_awake() {
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->add_on_update_event(this, &RenderComponent::_on_transform_update);
    }
}
void RenderComponent::on_destroy() {
    remove_object();
    auto tr = entity()->get_component<TransformComponent>();
    if (tr) {
        tr->remove_on_update_event(this);
    }
}
void RenderComponent::update_data() {
    update_object();
}
void RenderComponent::serialize_meta(ObjSerialize const &ser) const {
    ser.ar.start_array();
    for (auto const &i : _materials) {
        vstd::Guid guid;
        if (i) {
            guid = i->guid();
        } else {
            guid.reset();
        }
        ser.ar.value(guid);
    }
    ser.ar.end_array("mats");
    if (_mesh_ref) {
        auto guid = _mesh_ref->guid();
        if (guid) {
            ser.ar.value(guid, "mesh");
            // TODO: mark dependencies
            // ser.depended_resources.emplace(guid);
            // ser.depended_resources.emplace(guid);
        }
    }
}
void RenderComponent::deserialize_meta(ObjDeSerialize const &deser) {
    uint64_t size;
    if (deser.ar.start_array(size, "mats")) {
        _materials.reserve(size);
        for ([[maybe_unused]] auto i : vstd::range(size)) {
            vstd::Guid guid;
            if (!deser.ar.value(guid)) {
                _materials.emplace_back(nullptr);
            } else {
                auto res = get_resource(guid, true);
                if (res && res->is_type_of(TypeInfo::get<MaterialResource>())) {
                    _materials.emplace_back(std::move(res));
                } else {
                    _materials.emplace_back(nullptr);
                }
            }
        }
        deser.ar.end_scope();
    }
    vstd::Guid guid;
    if (deser.ar.value(guid, "mesh")) {
        auto res = get_resource(guid, true);
        if (res && res->is_type_of(TypeInfo::get<MeshResource>())) {
            _mesh_ref = RC<MeshResource>(std::move(res));
        }
    }
}

static bool material_is_emission(luisa::span<RC<MaterialResource> const> materials) {
    bool contained_emission = false;
    for (auto const &i : materials) {
        if (!i) [[unlikely]]
            continue;
        i->mat_data().visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, material::OpenPBR>) {
                for (auto const i : t.emission.luminance) {
                    if (i > 1e-3f) {
                        contained_emission = true;
                        return;
                    }
                }
            } else {
                for (auto const i : t.color) {
                    if (i > 1e-3f) {
                        contained_emission = true;
                        return;
                    }
                }
            }
        });
    }
    return contained_emission;
}
static luisa::vector<float> material_emissions(luisa::span<RC<MaterialResource> const> materials) {
    luisa::vector<float> vec;
    vec.resize(materials.size());
    size_t mat_index = 0;
    for (auto const &i : materials) {
        if (i) [[likely]] {
            float3 emission{};
            i->mat_data().visit([&]<typename T>(T const &t) {
                if constexpr (std::is_same_v<T, material::OpenPBR>) {
                    emission = float3{
                        t.emission.luminance[0],
                        t.emission.luminance[1],
                        t.emission.luminance[2]};

                } else {
                    emission = float3{
                        t.color[0],
                        t.color[1],
                        t.color[2]};
                }
            });

            float lum = dot(emission, float3(0.2126729, 0.7151522, 0.0721750));
            vec[mat_index] = lum;
        }
        ++mat_index;
    }
    return vec;
}
RenderComponent::~RenderComponent() {
    remove_object();
}

void RenderComponent::_update_shader_feature_bindings(
    luisa::vector<MaterialResource const *> &&materials) {
    if (auto scene = SceneManager::instance_ptr()) {
        for (auto const material : materials) {
            scene->bind_shader_feature_source(material, material->shader_feature_mask());
        }
        for (auto const material : _shader_feature_materials) {
            scene->unbind_shader_feature_source(material);
        }
    }
    _shader_feature_materials = std::move(materials);
}

void RenderComponent::_clear_shader_feature_bindings() {
    if (auto scene = SceneManager::instance_ptr()) {
        for (auto const material : _shader_feature_materials) {
            scene->unbind_shader_feature_source(material);
        }
    }
    _shader_feature_materials.clear();
}

void RenderComponent::_remove_tlas_idx() {
    std::lock_guard lck{render_comp_lists->_mtx};
    render_comp_lists->accel_ids.erase(_mesh_tlas_idx);
}
void RenderComponent::_add_tlas_idx() {
    std::lock_guard lck{render_comp_lists->_mtx};
    render_comp_lists->accel_ids.force_emplace(_mesh_tlas_idx, guid());
}

void RenderComponent::remove_object() {
    _clear_shader_feature_bindings();
    auto const sm = SceneManager::instance_ptr();
    if (_mesh_ref && _mesh_ref->device_mesh()) {
        _mesh_ref.reset();
    }
    auto dsp = vstd::scope_exit([&]() {
        if (_mesh_tlas_idx != ~0u) {
            _remove_tlas_idx();
        }
        _mesh_tlas_idx = ~0u;
        _mesh_light_idx = ~0u;
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
                _mesh_tlas_idx);
            break;
    }
}

RenderComponent::RenderComponent() {
    _mesh_light_idx = ~0u;
}
void RenderComponent::update_object(luisa::span<RC<MaterialResource> const> mats, MeshResource *mesh) {
    auto tr = entity()->get_component<TransformComponent>();
    if (!tr) {
        LUISA_WARNING("Transform component not found, renderer update failed.");
        return;
    }
    float4x4 const matrix = tr->trs_float();
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    if (mesh) {
        if (!mesh->empty()) {
            _mesh_ref.reset();
            _mesh_ref = mesh;
        }
    } else {
        mesh = _mesh_ref.get();
    }
    if (!mesh) {
        LUISA_WARNING("Mesh not loaded, renderer update failed.");
        return;
    }
    auto submesh_size = mesh->submesh_count();
    [[maybe_unused]] luisa::vector<RC<MaterialResource>> old_materials;
    if (!mats.empty()) {
        old_materials = std::move(_materials);
        auto setted_size = std::min(mats.size(), submesh_size);
        vstd::push_back_all(
            _materials,
            mats.subspan(0, setted_size));
    }
    _material_codes.clear();
    _material_codes.reserve(submesh_size);
    luisa::vector<MaterialResource const *> shader_feature_materials;
    shader_feature_materials.reserve(submesh_size);
    mesh->install();
    vstd::push_back_func(
        _material_codes,
        submesh_size,
        [&](size_t i) {
            if (i >= _materials.size())
                return MaterialResource::default_mat_code();
            auto &&mat = _materials[i];
            if (!mat || mat->loading_status() == EResourceLoadingStatus::Unloaded) {
                return MaterialResource::default_mat_code();
            } else {
                mat->install();
                if (mat->mat_code().value == ~0u) [[unlikely]] {
                    return MaterialResource::default_mat_code();
                }
                shader_feature_materials.emplace_back(mat.get());
                return mat->mat_code();
            }
        });
    if (_material_codes.size() < submesh_size) {
        auto default_mat = MaterialResource::default_mat_code();
        do {
            _material_codes.emplace_back(default_mat);
        } while (_material_codes.size() < submesh_size);
    }
    _update_shader_feature_bindings(std::move(shader_feature_materials));
    // TODO: change light type
    bool is_emission = material_is_emission(_materials);
    if (!render_device) return;

    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("RenderComponent::update_object can only be called in render-thread.");
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
                    _remove_tlas_idx();
                    auto const emissions = material_emissions(_materials);
                    _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        RC<DeviceMesh>{mesh->device_mesh()},
                        matrix,
                        emissions,
                        _material_codes);
                    _mesh_tlas_idx = Lights::instance()->mesh_lights.light_data[_mesh_light_idx].tlas_id;
                    _add_tlas_idx();
                    _type = ObjectRenderType::EmissionMesh;
                } else {
                    sm.accel_manager().set_mesh_instance(
                        _mesh_tlas_idx,
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh->mesh_data(),
                        _material_codes,
                        matrix);
                    _mesh_light_idx = ~0u;
                }
                break;
            case ObjectRenderType::EmissionMesh:
                if (!is_emission) {
                    Lights::instance()->remove_mesh_light(_mesh_light_idx);
                    _mesh_light_idx = ~0u;
                    _remove_tlas_idx();
                    _mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh->mesh_data(),
                        _material_codes,
                        matrix);
                    _add_tlas_idx();
                    _type = ObjectRenderType::Mesh;
                } else {
                    auto const emissions = material_emissions(_materials);
                    RC<DeviceMesh> m{mesh->device_mesh()};
                    Lights::instance()->update_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        _mesh_light_idx,
                        matrix,
                        emissions,
                        _material_codes,
                        &m);
                    _remove_tlas_idx();
                    _mesh_tlas_idx = Lights::instance()->mesh_lights.light_data[_mesh_light_idx].tlas_id;
                    _add_tlas_idx();
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
            auto const emissions = material_emissions(_materials);
            _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                RC<DeviceMesh>{mesh->device_mesh()},
                matrix,
                emissions,
                _material_codes);
            _mesh_tlas_idx = Lights::instance()->mesh_lights.light_data[_mesh_light_idx].tlas_id;
            _add_tlas_idx();
            _type = ObjectRenderType::EmissionMesh;
        } else {
            _mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                render_device->lc_main_cmd_list(),
                sm.host_upload_buffer(),
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue(),
                mesh->mesh_data(),
                _material_codes,
                matrix);
            _mesh_light_idx = ~0u;
            _add_tlas_idx();
            _type = ObjectRenderType::Mesh;
        }
    }
}
void RenderComponent::_update_object_pos(float4x4 const &matrix) {
    if (_mesh_tlas_idx == ~0u) [[unlikely]] {
        return;
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
        case ObjectRenderType::EmissionMesh: {
            auto const emissions = material_emissions(_materials);
            Lights::instance()->update_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                _mesh_light_idx,
                matrix,
                emissions,
                _material_codes);
        } break;
        case ObjectRenderType::Procedural:
            sm.accel_manager().set_procedural_instance(
                _mesh_tlas_idx,
                matrix,
                0xffu,
                true);
            break;
    }
}

uint RenderComponent::get_tlas_index() const {
    return _mesh_tlas_idx;
}

DECLARE_WORLD_OBJECT_REGISTER(RenderComponent);
}// namespace rbc::world
