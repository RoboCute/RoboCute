#include <rbc_graphics/object_types.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/vstl/common.h>
namespace rbc {
template<typename T, typename OpenPBR>
void serde_openpbr(
    T t,
    OpenPBR x) {
    auto serde_func = [&]<typename U>(U &u, char const *name) {
        using PureU = std::remove_cvref_t<U>;
        constexpr bool is_index = requires { u.index; };
        constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
        if constexpr (rbc::is_serializer_v<std::remove_cvref_t<T>>) {
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
        } else {
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
        }
    };
    serde_func(x.weight.base, "weight_base");
    serde_func(x.weight.diffuse_roughness, "weight_diffuse_roughness");
    serde_func(x.weight.specular, "weight_specular");
    serde_func(x.weight.metallic, "weight_metallic");
    serde_func(x.weight.metallic_roughness_tex, "weight_metallic_roughness_tex");
    serde_func(x.weight.subsurface, "weight_subsurface");
    serde_func(x.weight.transmission, "weight_transmission");
    serde_func(x.weight.thin_film, "weight_thin_film");
    serde_func(x.weight.fuzz, "weight_fuzz");
    serde_func(x.weight.coat, "weight_coat");
    serde_func(x.weight.diffraction, "weight_diffraction");
    serde_func(x.geometry.cutout_threshold, "geometry_cutout_threshold");
    serde_func(x.geometry.opacity, "geometry_opacity");
    serde_func(x.geometry.opacity_tex, "geometry_opacity_tex");
    serde_func(x.geometry.thickness, "geometry_thickness");
    serde_func(x.geometry.thin_walled, "geometry_thin_walled");
    serde_func(x.geometry.nested_priority, "geometry_nested_priority");
    serde_func(x.geometry.bump_scale, "geometry_bump_scale");
    serde_func(x.geometry.normal_tex, "geometry_normal_tex");
    serde_func(x.uvs.uv_scale, "uv_scale");
    serde_func(x.uvs.uv_offset, "uv_offset");
    serde_func(x.specular.specular_color, "specular_color");
    serde_func(x.specular.roughness, "specular_roughness");
    serde_func(x.specular.roughness_anisotropy, "specular_roughness_anisotropy");
    serde_func(x.specular.specular_anisotropy_level_tex, "specular_anisotropy_level_tex");
    serde_func(x.specular.roughness_anisotropy_angle, "specular_roughness_anisotropy_angle");
    serde_func(x.specular.specular_anisotropy_angle_tex, "specular_anisotropy_angle_tex");
    serde_func(x.specular.ior, "specular_ior");
    serde_func(x.emission.luminance, "emission_luminance");
    serde_func(x.emission.emission_tex, "emission_tex");
    serde_func(x.base.albedo, "base_albedo");
    serde_func(x.base.albedo_tex, "base_albedo_tex");
    serde_func(x.subsurface.subsurface_color, "subsurface_color");
    serde_func(x.subsurface.subsurface_radius, "subsurface_radius");
    serde_func(x.subsurface.subsurface_radius_scale, "subsurface_radius_scale");
    serde_func(x.subsurface.subsurface_scatter_anisotropy, "subsurface_scatter_anisotropy");
    serde_func(x.transmission.transmission_color, "transmission_color");
    serde_func(x.transmission.transmission_depth, "transmission_depth");
    serde_func(x.transmission.transmission_scatter, "transmission_scatter");
    serde_func(x.transmission.transmission_scatter_anisotropy, "transmission_scatter_anisotropy");
    serde_func(x.transmission.transmission_dispersion_scale, "transmission_dispersion_scale");
    serde_func(x.transmission.transmission_dispersion_abbe_number, "transmission_dispersion_abbe_number");
    serde_func(x.coat.coat_color, "coat_color");
    serde_func(x.coat.coat_roughness, "coat_roughness");
    serde_func(x.coat.coat_roughness_anisotropy, "coat_roughness_anisotropy");
    serde_func(x.coat.coat_roughness_anisotropy_angle, "coat_roughness_anisotropy_angle");
    serde_func(x.coat.coat_ior, "coat_ior");
    serde_func(x.coat.coat_darkening, "coat_darkening");
    serde_func(x.coat.coat_roughening, "coat_roughening");
    serde_func(x.fuzz.fuzz_color, "fuzz_color");
    serde_func(x.fuzz.fuzz_roughness, "fuzz_roughness");
    serde_func(x.diffraction.diffraction_color, "diffraction_color");
    serde_func(x.diffraction.diffraction_thickness, "diffraction_thickness");
    serde_func(x.diffraction.diffraction_inv_pitch_x, "diffraction_inv_pitch_x");
    serde_func(x.diffraction.diffraction_inv_pitch_y, "diffraction_inv_pitch_y");
    serde_func(x.diffraction.diffraction_angle, "diffraction_angle");
    serde_func(x.diffraction.diffraction_lobe_count, "diffraction_lobe_count");
    serde_func(x.diffraction.diffraction_type, "diffraction_type");
    serde_func(x.thin_film.thin_film_thickness, "thin_film_thickness");
    serde_func(x.thin_film.thin_film_ior, "thin_film_ior");
}

void MaterialStub::openpbr_json_ser(JsonSerializer &json_ser, material::OpenPBR const &mat) {
    serde_openpbr<JsonSerializer &, material::OpenPBR const &>(json_ser, mat);
}
void MaterialStub::openpbr_json_deser(JsonDeSerializer &json_deser, material::OpenPBR &mat) {
    serde_openpbr<JsonDeSerializer &, material::OpenPBR &>(json_deser, mat);
}
void MaterialStub::openpbr_json_ser(JsonSerializer &json_ser, material::Unlit const &mat) {
    LUISA_NOT_IMPLEMENTED();
}
void MaterialStub::openpbr_json_deser(JsonDeSerializer &json_deser, material::Unlit &mat) {
    LUISA_NOT_IMPLEMENTED();
}

void MaterialStub::craete_pbr_material() {
    mat_data.reset_as<material::OpenPBR>();
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
            auto &sm = SceneManager::instance();
            auto &render_device = RenderDevice::instance();
            if (mat_code.value == ~0u) {
                mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial, material::OpenPBR>(
                    t,
                    render_device.lc_main_cmd_list(),
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
        } else {
            LUISA_ERROR("Material type not supported.");
        }
    });
}
MaterialStub::~MaterialStub() {
    auto sm = SceneManager::instance_ptr();
    if (!sm) return;
    if (mat_code.value != ~0u)
        sm->mat_manager().discard_mat_instance(mat_code);
}
ObjectStub::~ObjectStub() {
    auto sm = SceneManager::instance_ptr();
    if (mesh_ref) {
        mesh_ref->tlas_ref_count--;
    }
    if (!sm) return;
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
LightStub::~LightStub() {
    auto lights = Lights::instance();
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
void ObjectStub::create_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> mats) {
    auto &render_device = RenderDevice::instance();
    auto &sm = SceneManager::instance();
    LUISA_ASSERT(!mesh_ref || mesh_tlas_idx != ~0U, "object already created.");
    mesh_ref = mesh;
    mesh_ref->tlas_ref_count++;

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
    if (material_is_emission(materials)) {
        mesh_light_idx = Lights::instance()->add_mesh_light_sync(
            render_device.lc_main_cmd_list(),
            mesh_ref,
            matrix,
            material_codes);
        type = ObjectRenderType::EmissionMesh;
    } else {
        mesh_tlas_idx = sm.accel_manager().emplace_mesh_instance(
            render_device.lc_main_cmd_list(),
            sm.host_upload_buffer(),
            sm.buffer_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue(),
            mesh_ref->mesh_data(),
            material_codes,
            matrix);
        type = ObjectRenderType::Mesh;
    }
}
ObjectStub::ObjectStub() {
    mesh_light_idx = ~0u;
}
void ObjectStub::update_object(luisa::float4x4 matrix, DeviceMesh *mesh, luisa::span<RC<RCBase> const> mats) {
    auto &render_device = RenderDevice::instance();
    auto &sm = SceneManager::instance();
    if (mesh_ref) {
        mesh_ref->tlas_ref_count--;
        mesh_ref.reset();
    }
    mesh_ref = mesh;
    mesh_ref->tlas_ref_count++;
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
    switch (type) {
        case ObjectRenderType::Mesh:
            if (is_emission) {
                sm.accel_manager().remove_mesh_instance(
                    sm.buffer_allocator(),
                    sm.buffer_uploader(),
                    mesh_tlas_idx);
                mesh_light_idx = Lights::instance()->add_mesh_light_sync(
                    render_device.lc_main_cmd_list(),
                    mesh_ref,
                    matrix,
                    material_codes);
                type = ObjectRenderType::EmissionMesh;
            } else {
                sm.accel_manager().set_mesh_instance(
                    mesh_tlas_idx,
                    render_device.lc_main_cmd_list(),
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
                    render_device.lc_main_cmd_list(),
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
                    render_device.lc_main_cmd_list(),
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
void ObjectStub::update_object_pos(luisa::float4x4 matrix) {
    auto &render_device = RenderDevice::instance();
    auto &sm = SceneManager::instance();
    switch (type) {
        case ObjectRenderType::Mesh: {
            sm.accel_manager().set_mesh_instance(
                render_device.lc_main_cmd_list(),
                sm.buffer_uploader(),
                mesh_tlas_idx,
                matrix, 0xff, true);
        } break;
        case ObjectRenderType::EmissionMesh:
            Lights::instance()->update_mesh_light_sync(
                render_device.lc_main_cmd_list(),
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