#include <rbc_graphics/object_types.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/vstl/common.h>
#include <rbc_graphics/mat_serde.h>
namespace rbc {

void MaterialStub::openpbr_json_ser(JsonSerializer &t, material::OpenPBR const &mat) {
    t._store("type"sv, "pbr");
    auto serde_func = [&]<typename U>(U &u, char const *name) {
        using PureU = std::remove_cvref_t<U>;
        constexpr bool is_index = requires { u.index; };
        constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
        if constexpr (is_index) {
            t._store(u.index, name);
        } else if constexpr (is_array) {
            t.start_array();
            for (auto &i : u) {
                t._store(i);
            }
            t.add_last_scope_to_object(name);
        } else {
            t._store(u, name);
        }
    };

    detail::serde_openpbr<material::OpenPBR const &>(mat, serde_func);
}
void MaterialStub::openpbr_json_deser(JsonDeSerializer &t, material::OpenPBR &mat) {
    luisa::string type;
    if (t._load(type, "type")) {
        if (type != "pbr") return;
    }
    auto serde_func = [&]<typename U>(U &u, char const *name) {
        using PureU = std::remove_cvref_t<U>;
        constexpr bool is_index = requires { u.index; };
        constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
        if constexpr (is_index) {
            t._load(u.index, name);
        } else if constexpr (is_array) {
            uint64_t size;
            if (!t.start_array(size, name))
                return;
            LUISA_ASSERT(size == u.size(), "Variable {} array size mismatch.", name);
            for (auto &i : u) {
                t._load(i);
            }
            t.end_scope();
        } else {
            t._load(u, name);
        }
    };
    detail::serde_openpbr<material::OpenPBR &>(mat, serde_func);
}
void MaterialStub::openpbr_json_ser(JsonSerializer &json_ser, material::Unlit const &mat) {
    LUISA_NOT_IMPLEMENTED();
}
void MaterialStub::openpbr_json_deser(JsonDeSerializer &json_deser, material::Unlit &mat) {
    LUISA_NOT_IMPLEMENTED();
}
MaterialStub::MaterialStub() {
    mat_code.value = ~0u;
}
void MaterialStub::craete_pbr_material() {
    LUISA_DEBUG_ASSERT(mat_code.value == ~0u);
    mat_data.reset_as<material::OpenPBR>();
}
void MaterialStub::remove_material() {
    if (mat_code.value == ~0u) return;
    auto sm = SceneManager::instance_ptr();
    if (sm && mat_code.value != ~0u)
        sm->mat_manager().discard_mat_instance(mat_code);
    mat_code.value = ~0u;
}
void MaterialStub::update_material(luisa::string_view json) {
    mat_data.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, material::OpenPBR>) {
            if (mat_code.value != ~0u && mat_code.get_type() != material::PolymorphicMaterial::index<material::OpenPBR>) [[unlikely]] {
                LUISA_ERROR("Material type mismatch.");
            }
            JsonDeSerializer deser(json);
            openpbr_json_deser(
                deser,
                t);

        } else {
            LUISA_ERROR("Material type not supported.");
        }

        auto render_device = RenderDevice::instance_ptr();
        auto &sm = SceneManager::instance();
        if (render_device) {
            if (mat_code.value == ~0u) {
                mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial, T>(
                    t,
                    render_device->lc_main_cmd_list(),
                    sm.bindless_allocator(),
                    sm.buffer_uploader(),
                    sm.dispose_queue());
            } else {
                sm.mat_manager().set_mat_instance(
                    mat_code,
                    sm.buffer_uploader(),
                    {(std::byte const *)&t,
                     sizeof(t)});
            }
        }
    });
}
MaterialStub::~MaterialStub() {
    remove_material();
}
void LightStub::remove_light() {
    if (id == ~0u) return;
    auto lights = Lights::instance();
    auto dsp_id = vstd::scope_exit([&]() {
        id = ~0u;
    });
    if (!lights) return;
    auto type = light_type;
    switch (type) {
        case rbc::LightType::Sphere:
            lights->remove_point_light(id);
            break;
        case rbc::LightType::Spot:
            lights->remove_spot_light(id);
            break;
        case rbc::LightType::Area:
            lights->remove_area_light(id);
            break;
        case rbc::LightType::Disk:
            lights->remove_disk_light(id);
            break;
        case rbc::LightType::Blas:
            lights->remove_mesh_light(id);
            break;
        default:
            return;
    }
}
LightStub::~LightStub() {
    remove_light();
}
void LightStub::add_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    light_type = LightType::Area;
    id = Lights::instance()->add_area_light(
        render_device.lc_main_cmd_list(),
        matrix,
        luminance,
        {}, {},
        visible);
}
void LightStub::add_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) {
    auto &render_device = RenderDevice::instance();
    light_type = LightType::Disk;
    id = Lights::instance()->add_disk_light(
        render_device.lc_main_cmd_list(),
        center,
        radius,
        luminance,
        forward_dir,
        visible);
}
void LightStub::add_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    light_type = LightType::Sphere;
    id = Lights::instance()->add_point_light(
        render_device.lc_main_cmd_list(),
        center,
        radius,
        luminance,
        visible);
}
void LightStub::add_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    auto &render_device = RenderDevice::instance();
    light_type = LightType::Spot;
    id = Lights::instance()->add_spot_light(
        render_device.lc_main_cmd_list(),
        center,
        radius,
        luminance,
        forward_dir,
        angle_atten_pow,
        small_angle_radians,
        angle_atten_pow,
        visible);
}
void LightStub::update_area_light(luisa::float4x4 matrix, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    LUISA_ASSERT(light_type == LightType::Area);
    Lights::instance()->update_area_light(
        render_device.lc_main_cmd_list(),
        id,
        matrix,
        luminance,
        {}, {},
        visible);
}
void LightStub::update_disk_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, bool visible) {
    auto &render_device = RenderDevice::instance();
    LUISA_ASSERT(light_type == LightType::Disk);
    Lights::instance()->update_disk_light(
        render_device.lc_main_cmd_list(),
        id,
        center,
        radius,
        luminance,
        forward_dir,
        visible);
}
void LightStub::update_point_light(luisa::float3 center, float radius, luisa::float3 luminance, bool visible) {
    auto &render_device = RenderDevice::instance();
    LUISA_ASSERT(light_type == LightType::Sphere);
    Lights::instance()->update_point_light(
        render_device.lc_main_cmd_list(),
        id,
        center,
        radius,
        luminance,
        visible);
}
void LightStub::update_spot_light(luisa::float3 center, float radius, luisa::float3 luminance, luisa::float3 forward_dir, float angle_radians, float small_angle_radians, float angle_atten_pow, bool visible) {
    auto &render_device = RenderDevice::instance();
    LUISA_ASSERT(light_type == LightType::Spot);
    Lights::instance()->update_spot_light(
        render_device.lc_main_cmd_list(),
        id,
        center,
        radius,
        luminance,
        forward_dir,
        angle_radians,
        small_angle_radians,
        angle_atten_pow,
        visible);
}
static bool material_is_emission(luisa::span<RC<MaterialStub> const> materials) {
    bool contained_emission = false;
    for (auto &i : materials) {
        static_cast<MaterialStub *>(i.get())->mat_data.visit([&]<typename T>(T const &t) {
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
ObjectStub::~ObjectStub() {
    remove_object();
}
void ObjectStub::remove_object() {
    auto sm = SceneManager::instance_ptr();
    if (mesh_ref) {
        mesh_ref.reset();
    }
    auto dsp = vstd::scope_exit([&]() {
        mesh_tlas_idx = ~0u;
    });
    if (!sm || mesh_tlas_idx == ~0u) return;
    switch (type) {
        case ObjectRenderType::Mesh:
            sm->accel_manager().remove_mesh_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                mesh_tlas_idx);
            break;
        case ObjectRenderType::EmissionMesh:
            if (Lights::instance()) {
                Lights::instance()->remove_mesh_light(mesh_light_idx);
            }
            break;
        case ObjectRenderType::Procedural:
            sm->accel_manager().remove_procedural_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                sm->dispose_queue(),
                procedural_idx);
            break;
    }
}

void ObjectStub::create_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> mats) {
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    LUISA_ASSERT(!mesh_ref || mesh_tlas_idx != ~0U, "object already created.");
    mesh_ref = mesh;

    vstd::push_back_func(
        materials,
        mats.size(),
        [&](size_t i) {
            return (MaterialStub *)(mats[i].get());
        });
    vstd::push_back_func(
        material_codes,
        materials.size(),
        [&](size_t i) {
            return materials[i]->mat_code;
        });
    mesh_ref->wait_finished();
    auto submesh_size = std::max<size_t>(1, mesh_ref->mesh_data()->submesh_offset.size());
    if (!(materials.size() == submesh_size)) [[unlikely]] {
        LUISA_ERROR("Submesh size {} mismatch with material size {}", submesh_size, materials.size());
    }
    if (render_device) {
        if (material_is_emission(materials)) {
            mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                render_device->lc_main_cmd_list(),
                mesh_ref,
                matrix,
                material_codes);
            type = ObjectRenderType::EmissionMesh;
        } else {
            mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                render_device->lc_main_cmd_list(),
                sm.host_upload_buffer(),
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue(),
                mesh_ref->mesh_data(),
                material_codes,
                matrix);
            type = ObjectRenderType::Mesh;
        }
    } else {
        mesh_tlas_idx = ~0u;
    }
}
ObjectStub::ObjectStub() {
    mesh_light_idx = ~0u;
}
void ObjectStub::update_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> mats) {
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    if (mesh_ref) {
        mesh_ref.reset();
    }
    mesh_ref = mesh;
    materials.clear();
    material_codes.clear();
    auto submesh_size = std::max<size_t>(1, mesh_ref->mesh_data()->submesh_offset.size());
    if (!(mats.size() == submesh_size)) [[unlikely]] {
        LUISA_ERROR("Submesh size {} mismatch with material size {}", submesh_size, mats.size());
    }
    vstd::push_back_func(
        materials,
        mats.size(),
        [&](size_t i) {
            return static_cast<MaterialStub *>(mats[i].get());
        });
    vstd::push_back_func(
        material_codes,
        materials.size(),
        [&](size_t i) {
            return materials[i]->mat_code;
        });
    mesh_ref->wait_finished();
    // TODO: change light type
    bool is_emission = material_is_emission(materials);
    if (render_device) {
        switch (type) {
            case ObjectRenderType::Mesh:
                if (is_emission) {
                    sm.accel_manager().remove_mesh_instance(
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        mesh_tlas_idx);
                    mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        mesh_ref,
                        matrix,
                        material_codes);
                    type = ObjectRenderType::EmissionMesh;
                } else {
                    sm.accel_manager().set_mesh_instance(
                        mesh_tlas_idx,
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh_ref->mesh_data(),
                        material_codes,
                        matrix);
                }
                break;
            case ObjectRenderType::EmissionMesh:
                if (!is_emission) {
                    Lights::instance()->remove_mesh_light(mesh_light_idx);
                    mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
                        render_device->lc_main_cmd_list(),
                        sm.host_upload_buffer(),
                        sm.buffer_allocator(),
                        sm.buffer_uploader(),
                        sm.dispose_queue(),
                        mesh_ref->mesh_data(),
                        material_codes,
                        matrix);
                    type = ObjectRenderType::Mesh;
                } else {
                    Lights::instance()->update_mesh_light_sync(
                        render_device->lc_main_cmd_list(),
                        mesh_light_idx,
                        matrix,
                        material_codes,
                        &mesh_ref);
                }
                break;
            case ObjectRenderType::Procedural: {
                LUISA_ERROR("Procedural not supported.");
            } break;
        }
    }
}
void ObjectStub::update_object_pos(luisa::float4x4 matrix) {
    auto render_device = RenderDevice::instance_ptr();
    auto &sm = SceneManager::instance();
    if (render_device) {
        switch (type) {
            case ObjectRenderType::Mesh: {
                sm.accel_manager().set_mesh_instance(
                    render_device->lc_main_cmd_list(),
                    sm.buffer_uploader(),
                    mesh_tlas_idx,
                    matrix, 0xff, true);
            } break;
            case ObjectRenderType::EmissionMesh:
                Lights::instance()->update_mesh_light_sync(
                    render_device->lc_main_cmd_list(),
                    mesh_light_idx,
                    matrix,
                    material_codes);
                break;
            case ObjectRenderType::Procedural:
                sm.accel_manager().set_procedural_instance(
                    procedural_idx,
                    matrix,
                    0xffu,
                    true);
                break;
        }
    }
}

uint ObjectStub::get_tlas_index() const {
    switch (type) {
        case ObjectRenderType::Mesh:
            return mesh_tlas_idx;
        case ObjectRenderType::EmissionMesh:
            return Lights::instance()->mesh_lights.light_data[mesh_light_idx].tlas_id;
        case ObjectRenderType::Procedural:
            return procedural_idx;
        default:
            return ~0u;
    }
}
}// namespace rbc