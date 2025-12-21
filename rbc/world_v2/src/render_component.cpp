#include <rbc_world_v2/render_component.h>
#include <rbc_world_v2/components/transform.h>
#include <rbc_world_v2/resource_loader.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_world_v2/resources/material.h>
#include <rbc_world_v2/resources/mesh.h>
#include <rbc_graphics/render_device.h>
#include <rbc_world_v2/entity.h>
/*
TODO: only should update light bvh while in ray-tracing mode
*/
namespace rbc::world {
void RenderComponent::_on_transform_update() {
    if (_mesh_tlas_idx != ~0u) {
        auto tr = entity()->get_component<TransformComponent>();
        if (!tr) return;
        _update_object_pos(tr->trs_float());
    }
}
void RenderComponent::on_awake() {
    add_event(WorldEventType::OnTransformUpdate, &RenderComponent::_on_transform_update);
}
void RenderComponent::on_destroy() {
    remove_object();
}
void RenderComponent::serialize_meta(ObjSerialize const &ser) const {
    auto &ser_obj = ser.ser;
    ser_obj.start_array();
    for (auto &i : _materials) {
        vstd::Guid guid;
        if (i) {
            guid = i->guid();
        } else {
            guid.reset();
        }
        ser_obj._store(guid);
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
void RenderComponent::deserialize_meta(ObjDeSerialize const &ser) {
    auto &obj = ser.ser;
    uint64_t size;
    if (obj.start_array(size, "mats")) {
        _materials.reserve(size);
        for (auto &i : vstd::range(size)) {
            vstd::Guid guid;
            if (!obj._load(guid)) {
                _materials.emplace_back(nullptr);
            } else {
                auto obj = load_resource(guid, true);
                if (obj && obj->is_type_of(TypeInfo::get<MaterialResource>())) {
                    _materials.emplace_back(std::move(obj));
                } else {
                    _materials.emplace_back(nullptr);
                }
            }
        }
        obj.end_scope();
    }
    vstd::Guid guid;
    if (obj._load(guid, "mesh")) {
        auto obj = load_resource(guid, true);
        if (obj && obj->is_type_of(TypeInfo::get<MeshResource>())) {
            _mesh_ref = RC<MeshResource>(std::move(obj));
        }
    }
}

static bool material_is_emission(luisa::span<RC<MaterialResource> const> materials) {
    bool contained_emission = false;
    for (auto &i : materials) {
        if (!i) [[unlikely]]
            continue;
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
static luisa::vector<float> material_emissions(luisa::span<RC<MaterialResource> const> materials) {
    luisa::vector<float> vec;
    vec.resize(materials.size());
    size_t mat_index = 0;
    for (auto &i : materials) {
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
};
RenderComponent::~RenderComponent() {
    remove_object();
}
void RenderComponent::remove_object() {
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

RenderComponent::RenderComponent(Entity *entity) : ComponentDerive<RenderComponent>(entity) {
    _mesh_light_idx = ~0u;
}
void RenderComponent::update_object(luisa::span<RC<MaterialResource> const> mats, MeshResource *mesh) {
    auto tr = entity()->get_component<TransformComponent>();
    if (!tr) {
        LUISA_WARNING("Transform component not found, renderer update failed.");
        return;
    }
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
    if (!mesh) {
        LUISA_WARNING("Mesh not loaded, renderer update failed.");
        return;
    }
    mesh->wait_load_finished();
    auto submesh_size = mesh->submesh_count();
    if (!mats.empty()) {
        _materials.clear();
        auto setted_size = std::min(mats.size(), submesh_size);
        vstd::push_back_all(
            _materials,
            mats.subspan(0, setted_size));
    }
    _material_codes.clear();
    _material_codes.reserve(submesh_size);
    vstd::push_back_func(
        _material_codes,
        std::min(submesh_size, _materials.size()),
        [&](size_t i) {
            auto &&mat = _materials[i];
            if (!mat || mat->empty()) {
                return MaterialResource::default_mat_code();
            } else {
                mat->init_device_resource();
                return mat->mat_code();
            }
        });
    if (_material_codes.size() < submesh_size) {
        auto default_mat = MaterialResource::default_mat_code();
        do {
            _material_codes.emplace_back(default_mat);
        } while (_material_codes.size() < submesh_size);
    }
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
                    auto emissions = material_emissions(_materials);
                    _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        mesh->device_mesh(),
                        matrix,
                        emissions,
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
                    auto emissions = material_emissions(_materials);
                    Lights::instance()->update_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        _mesh_light_idx,
                        matrix,
                        emissions,
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
            auto emissions = material_emissions(_materials);
            _mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                mesh->device_mesh(),
                matrix,
                emissions,
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
void RenderComponent::_update_object_pos(float4x4 matrix) {
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
        case ObjectRenderType::EmissionMesh: {
            auto emissions = material_emissions(_materials);
            Lights::instance()->update_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                _mesh_light_idx,
                matrix,
                emissions,
                _material_codes);
        } break;
        case ObjectRenderType::Procedural:
            sm.accel_manager().set_procedural_instance(
                _procedural_idx,
                matrix,
                0xffu,
                true);
            break;
    }
}

uint RenderComponent::get_tlas_index() const {
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

DECLARE_WORLD_COMPONENT_REGISTER(RenderComponent);
}// namespace rbc::world