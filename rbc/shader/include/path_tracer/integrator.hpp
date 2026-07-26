#pragma once
/////////// Allowed macros:
// #define PT_CONFIDENCE_ACCUM
// #define PT_SCREEN_RAY_DIFF
// #define PT_MOTION_VECTORS
#include <bsdfs/polymorphic.hpp>
#include <geometry/vertices.hpp>
#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <utils/onb.hpp>
#include <lighting/importance_sampling.hpp>
#include <spectrum/spectrum.hpp>
#include <material/mats.hpp>
#include <material/procedural_mats.hpp>
using namespace luisa::shader;

constexpr float const denoise_min_albedo = 1e-1f;
inline float3 clamp_denoise_albedo(float3 albedo) {
    float max_val = max(albedo.x, max(albedo.y, albedo.z)) * denoise_min_albedo;
    return max(albedo, float3(max_val));
}
struct IntegratorResult {
    float3 world_pos;
    float3 new_ray_offset;
#ifdef PT_MOTION_VECTORS
    float3 last_local_pos;
#endif
    float3 plane_normal;// cross(v0 - v1, v0 - v2)
    float3 normal;      // bump texture affected normal
    float3 emission = 0.0f;
    float3 albedo = denoise_min_albedo;
    float2 uv;
    float roughness;
    uint user_id;
    mtl::BSDFFlags sample_flags = mtl::BSDFFlags::None;
    float eta = 1.0f;
};
namespace integrator {

/// Compute MIS weight for indirect ray hitting a light source
/// Returns modified emission and updates continue_loop flag
inline float3 compute_light_mis(
    float3 emission,
    geometry::InstanceInfo inst_info,
    float3 input_pos,
    float pdf_bsdf,
    std::array<float3, 3> vert_poses,
    float3 vertices_normal,
    float ray_t,
    float3 input_dir,
    bool &continue_loop) {

    uint light_mask = inst_info.get_light_mask();
    uint light_id = inst_info.get_light_id();
    float3 result_emission = emission;

    switch (light_mask) {
        case lighting::LightTypes::PointLight: {
            auto point_light = g_buffer_heap.uniform_idx_buffer_read<lighting::PointLight>(heap_indices::point_lights_heap_idx, light_id);
            // triangle: longest edge is c, near angle is b, on angle's other side is a
            auto wi_length = length(point_light.pos() - input_pos);
            if (wi_length > point_light.radius()) {
                float half_light_angle = asin(point_light.radius() / wi_length);
                float cos_theta_max = cos(half_light_angle);
                auto pdf_light = 1.0f / (2 * pi * (1.0f - cos_theta_max));
                auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
                result_emission *= mis;
            }
            continue_loop = false;
        } break;
        case lighting::LightTypes::SpotLight: {
            auto spot_light = g_buffer_heap.uniform_idx_buffer_read<lighting::SpotLight>(heap_indices::spot_lights_heap_idx, light_id);
            auto wi_light = spot_light.pos() - input_pos;
            // triangle: longest edge is c, near angle is b, on angle's other side is a
            auto wi_length = length(wi_light);
            wi_light = wi_light / wi_length;
            if (wi_length > spot_light.radius()) {
                float half_light_angle = asin(spot_light.radius() / wi_length);
                float cos_theta_max = cos(half_light_angle);
                auto pdf_light = 1.0f / (2 * pi * (1.0f - cos_theta_max));
                auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
                float angle = acos(dot(-input_dir, float3(spot_light.forward_dir)));
                angle = saturate((angle - spot_light.angle_radian) / (spot_light.small_angle_radian - spot_light.angle_radian));
                angle = pow(angle, spot_light.angle_atten_power);
                result_emission *= angle;
                result_emission *= mis;
            }
            continue_loop = false;
        } break;
        case lighting::LightTypes::AreaLight: {
            float a = distance(vert_poses[0], vert_poses[1]);
            float b = distance(vert_poses[0], vert_poses[2]);
            float c = distance(vert_poses[1], vert_poses[2]);
            float p = (a + b + c) / 2.0f;
            float area = sqrt(p * (p - a) * (p - b) * (p - c)) * 2.f;
            auto pdf_light = (ray_t * ray_t) / max(area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
            auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
            result_emission *= mis;
            continue_loop = false;
        } break;
        case lighting::LightTypes::TriangleLight: {
            float a = distance(vert_poses[0], vert_poses[1]);
            float b = distance(vert_poses[0], vert_poses[2]);
            float c = distance(vert_poses[1], vert_poses[2]);
            float p = (a + b + c) / 2.0f;
            float area = sqrt(p * (p - a) * (p - b) * (p - c));
            auto pdf_light = (ray_t * ray_t) / max(area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
            auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
            result_emission *= mis;
        } break;
        case lighting::LightTypes::DiskLight: {
            auto disk_light = g_buffer_heap.uniform_idx_buffer_read<lighting::DiskLight>(heap_indices::disk_lights_heap_idx, light_id);
            auto pdf_light = (ray_t * ray_t) / max(disk_light.area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
            auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
            result_emission *= mis;
            continue_loop = false;
        } break;
    }
    return result_emission;
}

/// Perform BSDF light importance sampling
/// Returns the radiance result and distance to light
inline lighting::LightISResult perform_light_importance_sampling_normal(
    float3x3 resource_to_rec2020_mat,
    SpectrumArg &spectrum_arg,
    auto &bsdf_eval_func,
    auto &sampler,
    float3 world_pos,
    float3 plane_normal,
    float3 new_dir,
    float roughness,
    bool di_use_specular,
    bool is_primary_ray,
    float3x3 inst_normal_transform,
    std::array<float3, 3> vert_normals,
    std::array<float3, 3> vert_poses,
    auto hit,
    float3x3 world_2_sky_mat,
    lighting::BindlessIndices auto const &bdls_indices) {

    bool need_flip = false;
    float3 offset_normal = plane_normal;
    float3 di_normal = plane_normal;

    if (dot(new_dir, offset_normal) < 0.f) {
        offset_normal = -offset_normal;
        need_flip = true;
    }
    if (dot(new_dir, di_normal) < 0.f) {
        di_normal = -di_normal;
    }

    float3 di_world_pos = world_pos;

    // RAY TRACING GEMS II
    // CHAPTER 4. HACKING THE SHADOW TERMINATOR
    float3 nA = normalize(inst_normal_transform * vert_normals[0]);
    float3 nB = normalize(inst_normal_transform * vert_normals[1]);
    float3 nC = normalize(inst_normal_transform * vert_normals[2]);
    if (need_flip) {
        nA = -nA;
        nB = -nB;
        nC = -nC;
    }
    // get distance vectors from triangle vertices
    float3 tmpu = di_world_pos - vert_poses[0];
    float3 tmpv = di_world_pos - vert_poses[1];
    float3 tmpw = di_world_pos - vert_poses[2];
    // project these onto the tangent planes
    // defined by the shading normals
    tmpu -= min(0.0f, dot(tmpu, nA)) * nA;
    tmpv -= min(0.0f, dot(tmpv, nB)) * nB;
    tmpw -= min(0.0f, dot(tmpw, nC)) * nC;
    // finally P' is the barycentric mean of these three
    di_world_pos += hit.interpolate(tmpu, tmpv, tmpw);

    di_world_pos = sampling::offset_ray_origin(di_world_pos, offset_normal);

    auto is_result = lighting::bsdf_light_importance_sampling(
        resource_to_rec2020_mat,
        spectrum_arg,
        bsdf_eval_func,
        sampler,
        di_world_pos,
        di_normal,
        world_2_sky_mat,
        bdls_indices,
        new_dir,
        roughness,
        di_use_specular,
        is_primary_ray);

    return is_result;
}

/// Perform BSDF light importance sampling (simplified version without shadow terminator fix)
/// Returns the radiance result and distance to light
inline lighting::LightISResult perform_light_importance_sampling(
    float3x3 resource_to_rec2020_mat,
    SpectrumArg &spectrum_arg,
    auto const &bsdf_eval_func,
    auto &sampler,
    float3 world_pos,
    float3 plane_normal,
    float3 shading_normal,
    float3 new_dir,
    float roughness,
    bool di_use_specular,
    bool is_primary_ray,
    float3x3 world_2_sky_mat,
    lighting::BindlessIndices auto const &bdls_indices) {

    // Simplified version without shadow terminator fix
    float3 offset_normal = plane_normal;
    float3 di_normal = shading_normal;
    if (dot(new_dir, offset_normal) < 0.f) {
        offset_normal = -offset_normal;
    }
    if (dot(new_dir, di_normal) < 0.f) {
        di_normal = -di_normal;
    }
    float3 di_world_pos = sampling::offset_ray_origin(world_pos, offset_normal);

    auto is_result = lighting::bsdf_light_importance_sampling(
        resource_to_rec2020_mat,
        spectrum_arg,
        bsdf_eval_func,
        sampler,
        di_world_pos,
        di_normal,
        world_2_sky_mat,
        bdls_indices,
        new_dir,
        roughness,
        di_use_specular,
        is_primary_ray);

    return is_result;
}

}// namespace integrator

static IntegratorResult sample_material(
    auto &sampler,
    auto &volume_stack,
    auto &hit,
    auto procedural_geometry,
    SpectrumArg &spectrum_arg,
    float3x3 world_2_sky_mat,
    vt::VTMeta vt_meta,
    float lobe_rand,
    float3 input_pos,
    float3 &beta,
    bool &continue_loop,
    bool evaluate_bsdf,
    bool need_albedo,
    //////////// texture grad
    uint &texture_filter,
#ifdef PT_SCREEN_RAY_DIFF
    float2 tex_grad_scale,
#endif
    float2 &ddx,
    float2 &ddy,
#ifdef PT_SCREEN_RAY_DIFF
    float3 up_ray_dir,
    float3 right_ray_dir,
#endif

    mtl::ShadingDetail &detail,
    lighting::BindlessIndices auto const &bdls_indices,
    //////////// out
    bool importance_sampling,
    float3x3 resource_to_rec2020_mat,
    float3 &di_result,
    float &di_dist,
    float &pdf_bsdf,
    float3 &new_dir
#ifdef PT_MOTION_VECTORS
    ,
    bool require_last_local_pos
#endif
    ,
    bool &reject,
    uint &mat_id) {
    material::MatMeta mat_meta;
    geometry::InstanceInfo inst_info;
    bool is_indirect_ray = detail != mtl::ShadingDetail::Default;
    auto user_id = g_accel.instance_user_id(hit.inst);
    float3 world_pos;
    std::array<float2, 4> uv;
    float3 vertices_normal;
    float3 plane_normal;
    float3 input_dir;
    float ray_t;
    mtl::Onb vertices_onb;
    float4x4 inst_transform;
    bool contained_normal = false;
    bool contained_tangent = false;
    mtl::openpbr::BasicParameter basic_param;
    std::array<float3, 3> vert_poses;
    std::array<float3, 3> vert_normals;
#ifdef PT_MOTION_VECTORS
    float3 last_local_pos;
#endif
    bool hit_triangle = true;
    if constexpr (requires { hit.hit_triangle(); }) {
        hit_triangle = hit.hit_triangle();
    }
    uint uv_count = 0;
    uint boundary_id = max_uint32;
    IntegratorResult r;

    if (hit_triangle) {
        inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
        geometry::Triangle triangle;
        auto vertices = geometry::read_vertices(g_buffer_heap, hit.prim, inst_info.mesh, contained_normal, contained_tangent, uv_count, triangle);
        mat_meta = material::mat_meta(g_buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, hit.prim);
        mat_id = material::to_mat_code(mat_meta);
        inst_transform = g_accel.instance_transform(hit.inst);
        auto local_pos = hit.interpolate(vertices[0].pos, vertices[1].pos, vertices[2].pos);
#ifdef PT_MOTION_VECTORS
        last_local_pos = local_pos;
        if (require_last_local_pos && inst_info.last_vertex_heap_idx != uint(-1)) {
            float3 p0 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[0]);
            float3 p1 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[1]);
            float3 p2 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[2]);
            last_local_pos = hit.interpolate(p0, p1, p2);
        }
#endif
        for (int i = 0; i < 3; ++i) {
            vert_poses[i] = (inst_transform * float4(vertices[i].pos, 1)).xyz;
            vert_normals[i] = vertices[i].normal;
        }
        plane_normal = cross(vert_poses[0] - vert_poses[1], vert_poses[0] - vert_poses[2]);
        plane_normal = normalize(plane_normal);
        world_pos = (inst_transform * float4(local_pos, 1)).xyz;
        for (int i = 0; i < uv_count; ++i) {
            uv[i] = hit.interpolate(vertices[0].uvs[i], vertices[1].uvs[i], vertices[2].uvs[i]);
        }

        input_dir = world_pos - input_pos;
        ray_t = length(input_dir);
        hit.ray_t = ray_t;
        input_dir = input_dir / ray_t;

#ifdef PT_SCREEN_RAY_DIFF
        if (!is_indirect_ray) {
            auto right_bary = sampling::ray_tri_bary(
                input_pos,
                right_ray_dir,
                vert_poses[0],
                vert_poses[1],
                vert_poses[2]);
            auto up_bary = sampling::ray_tri_bary(
                input_pos,
                up_ray_dir,
                vert_poses[0],
                vert_poses[1],
                vert_poses[2]);
            auto right_uv = interpolate(right_bary, vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);
            auto up_uv = interpolate(up_bary, vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);
            ddx = (right_uv - uv[0]) * tex_grad_scale.x;
            ddy = (up_uv - uv[0]) * tex_grad_scale.y;
        }
#endif

        if (contained_normal) {
            vertices_normal = hit.interpolate(vertices[0].normal, vertices[1].normal, vertices[2].normal);
            vertices_normal = normalize(mtl::make_normal_transform(inst_transform) * vertices_normal);
            if (dot(input_dir, plane_normal) * dot(input_dir, vertices_normal) < 0.0f) {
                vertices_normal = plane_normal;
            }
        } else {
            vertices_normal = plane_normal;
        }

        if (contained_tangent) {
            auto tangent = hit.interpolate(vertices[0].tangent, vertices[1].tangent, vertices[2].tangent);
            basic_param.geometry.onb.tangent = normalize(inst_transform * float4(tangent.xyz, 0)).xyz;
            basic_param.geometry.onb.bitangent = normalize(cross(
                vertices_normal,
                basic_param.geometry.onb.tangent)) * tangent.w;
            basic_param.geometry.onb.normal = vertices_normal;
        } else {
            basic_param.geometry.onb = mtl::Onb(vertices_normal);
        }

        vertices_onb = basic_param.geometry.onb;
        if (dot(input_dir, basic_param.geometry.onb.normal) >= 0.0f) {
            basic_param.geometry.onb.normal = -basic_param.geometry.onb.normal;
            basic_param.geometry.onb.tangent = -basic_param.geometry.onb.tangent;
        }
        continue_loop = transform_to_params(
            g_buffer_heap,
            g_image_heap,
            mat_meta,
            basic_param,
            texture_filter,
            vt_meta,
            uv,
            uv_count,
            float4(ddx, ddy),
            input_dir,
            world_pos,
            reject);

    } else {
        plane_normal = float3(procedural_geometry.normal);
        vertices_normal = plane_normal;
        ray_t = hit.ray_t;
        world_pos = (ray_t - 1e-3f) * new_dir + input_pos + vertices_normal * 1e-4f;
        input_dir = normalize(world_pos - input_pos);
        basic_param.geometry.onb = mtl::Onb(vertices_normal);
        vertices_onb = basic_param.geometry.onb;
        continue_loop = ray_t > 0;
        if (continue_loop) {
            continue_loop = material::procedural_transform_to_params(
                g_buffer_heap,
                g_image_heap,
                procedural_geometry.procedural_id,
                basic_param,
                input_dir,
                world_pos,
                reject);
        }
    }

    auto init_spectrum_colors = [&](auto &param) {
        if constexpr (requires { param.emission; })
            param.emission.luminance.init_emission(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.base; })
            param.base.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.specular; })
            param.specular.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.diffraction; })
            if (basic_param.weight.diffraction > 0.0f)
                param.diffraction.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.coat; })
            if (basic_param.weight.coat > 0.0f)
                param.coat.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.fuzz; })
            if (basic_param.weight.fuzz > 0.0f)
                param.fuzz.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
        if constexpr (requires { param.subsurface; })
            if (basic_param.weight.subsurface > 0.0f) {
                param.subsurface.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
                param.subsurface.radius = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, param.subsurface.radius);
            }
        if constexpr (requires { param.transmission; })
            if (basic_param.weight.transmission > 0.0f) {
                param.transmission.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
                if (any(param.transmission.scatter != 0.0f)) {
                    param.transmission.scatter = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, param.transmission.scatter);
                }
            }
    };

    bool entering = dot(input_dir, plane_normal) < 0.0f;
    float3 wi = -basic_param.geometry.onb.to_local(input_dir);
#ifdef PT_MOTION_VECTORS
    r.last_local_pos = last_local_pos;
#endif
    r.world_pos = world_pos;
    r.new_ray_offset = 0.0f;
    r.user_id = user_id;
    r.normal = basic_param.geometry.onb.normal;
    r.plane_normal = plane_normal;
    r.roughness = basic_param.specular.roughness * (1.0f - 0.8f * basic_param.specular.roughness_anisotropy);
    r.albedo = float3(0);
    r.uv = uv[0];

    bool false_medium_boundary = false;
    bool boundary_uses_subsurface = false;
    if (hit_triangle &&
        !basic_param.geometry.thin_walled &&
        (basic_param.weight.transmission > 0.0f || basic_param.weight.subsurface > 0.0f)) {
        boundary_id = hit.inst;
        boundary_uses_subsurface = basic_param.weight.transmission <= 0.0f;
        mtl::Volume boundary;
        boundary.boundary_id = boundary_id;
        boundary.nested_priority = basic_param.geometry.nested_priority;
        false_medium_boundary = !mtl::active_medium_boundary_is_true(
            volume_stack,
            boundary,
            entering);
    }

    if (false_medium_boundary && !entering) {
        mtl::active_medium_remove(volume_stack, boundary_id);
        r.sample_flags = mtl::BSDFFlags::NoMediumChange;
        new_dir = input_dir;
        di_result = 0.0f;
        di_dist = 0.0f;
        return r;
    }
    init_spectrum_colors(basic_param);

    return mtl::PolymorphicBSDF::visit(std::to_underlying(mtl::detect_polymorphic_bsdf_type(basic_param.weight)), [&]<class ins>() {
        using type_pairs = typename ins::type;
        using MatBSDF = typename type_pairs::first_type;
        using MatExtraParameter = typename type_pairs::second_type;

        MatExtraParameter extra_param;

        if constexpr (requires { extra_param.coat; }) {
            extra_param.coat.coat_onb = vertices_onb;
        }
        if (hit_triangle) {
            transform_to_params(
                g_buffer_heap,
                g_image_heap,
                mat_meta,
                extra_param,
                texture_filter,
                vt_meta,
                uv,
                uv_count,
                float4(ddx, ddy),
                input_dir,
                world_pos,
                reject);
        } else {
            continue_loop = material::procedural_transform_to_params(
                g_buffer_heap,
                g_image_heap,
                procedural_geometry.procedural_id,
                extra_param,
                input_dir,
                world_pos,
                reject);
        }

        init_spectrum_colors(extra_param);

        auto fill_medium = [&](mtl::Volume& medium, bool diffuse) {
            bool has_medium = false;
            if (diffuse) {
                if constexpr (requires { extra_param.subsurface; }) {
                    medium.fill_from_subsurface(extra_param.subsurface);
                    has_medium = true;
                }
            } else {
                if constexpr (requires { extra_param.transmission; }) {
                    medium.fill_from_transmission(extra_param.transmission);
                    has_medium = true;
                }
            }
            return has_medium;
        };

        if (false_medium_boundary) {
            mtl::Volume medium;
            if (!fill_medium(medium, boundary_uses_subsurface)) {
                continue_loop = false;
                beta = 0.0f;
                return r;
            }
            medium.ior = basic_param.specular.ior;
            bool selected_wavelength_now = false;
            if (!boundary_uses_subsurface) {
                if constexpr (requires { extra_param.transmission; }) {
                    if (extra_param.transmission.dispersion_scale > 0.0f) {
                        selected_wavelength_now = !spectrum_arg.selected_wavelength;
                        spectrum_arg.selected_wavelength = true;
                        float hero_lambda = spectrum_arg.lambda[spectrum_arg.hero_index];
                        spectrum_arg.lambda = hero_lambda;
                        medium.ior = mtl::dispersion_ior(
                            basic_param.specular.ior,
                            extra_param.transmission.dispersion_abbe_number,
                            extra_param.transmission.dispersion_scale,
                            hero_lambda);
                    }
                }
            }
            medium.boundary_id = boundary_id;
            medium.nested_priority = basic_param.geometry.nested_priority;
            if (!mtl::active_medium_insert(volume_stack, medium)) {
                continue_loop = false;
                beta = 0.0f;
                return r;
            }
            if (selected_wavelength_now) {
                mtl::collapse_active_medium_wavelengths(
                    volume_stack,
                    spectrum_arg.hero_index);
            }
            r.sample_flags = mtl::BSDFFlags::NoMediumChange;
            new_dir = input_dir;
            di_result = 0.0f;
            di_dist = 0.0f;
            return r;
        }

        if (basic_param.geometry.thin_walled || dot(input_dir, basic_param.geometry.onb.normal) < 0) {
            r.emission = basic_param.emission.luminance.spectral();
            if constexpr (requires { extra_param.coat; }) {
                r.emission *= lerp(float3(1.0f), extra_param.coat.color.spectral(), basic_param.weight.coat);
            }
        }

        mtl::BSDFContext<MatExtraParameter, mtl::TransportMode::Radiance> bsdf_context{
            basic_param,
            extra_param,
            spectrum_arg.lambda,
            detail};
        bsdf_context.entering = entering;

        if (basic_param.geometry.thin_walled) {
            if (!volume_stack.empty()) {
                bsdf_context.inv_out_ior = rcp(volume_stack.back().ior);
            }
        } else {
            bsdf_context.inv_out_ior = rcp(mtl::active_medium_ior_across_boundary(
                volume_stack,
                boundary_id,
                entering));
        }

        bsdf_context.selected_wavelength = spectrum_arg.selected_wavelength;
        bsdf_context.hero_wavelength_index = spectrum_arg.hero_index;

        bsdf_context.rand = float3(sampler.next2f(g_buffer_heap), lobe_rand);
        bsdf_context.init(basic_param, extra_param);

        MatBSDF bsdf;

        bool di_use_specular = false;

        auto bsdf_eval_func = [&](float3 light_dir) {
            float3 wo = basic_param.geometry.onb.to_local(light_dir);

            auto old_flags = bsdf_context.sampling_flags;
            if (dot(vertices_normal, light_dir) *
                    dot(vertices_normal, input_dir) <
                0) {
                if (!mtl::is_reflective(r.sample_flags)) return float4{0.0f};
                bsdf_context.sampling_flags = mtl::BSDFFlags(old_flags & mtl::BSDFFlags::Reflection);
            } else {
                if (!mtl::is_transmissive(r.sample_flags)) return float4{0.0f};
                bsdf_context.sampling_flags = mtl::BSDFFlags(old_flags & mtl::BSDFFlags::Transmission);
            }
            auto eval_result = bsdf.eval(wi, wo, bsdf_context);
            auto pdf = bsdf.pdf(wi, wo, bsdf_context);
            bsdf_context.sampling_flags = old_flags;

            if (!eval_result ||
                any(eval_result.val < 0.f) ||
                !all(is_finite(eval_result.val)) ||
                !is_finite(pdf) || pdf <= 0.0f) {
                return float4{0.0f};
            }
            return float4{eval_result.val, pdf};
        };

        if (is_indirect_ray && hit_triangle) {// indirect ray
            r.emission = integrator::compute_light_mis(
                r.emission,
                inst_info,
                input_pos,
                pdf_bsdf,
                vert_poses,
                vertices_normal,
                ray_t,
                input_dir,
                continue_loop);
        }
        if (!evaluate_bsdf) return r;
        if constexpr (requires {
                          bsdf.prepare_interaction(
                              world_pos,
                              input_pos,
                              hit_triangle && volume_stack.empty());
                      }) {
            bsdf.prepare_interaction(
                world_pos,
                input_pos,
                hit_triangle && volume_stack.empty());
        }
        bsdf.init(wi, basic_param, extra_param, bsdf_context);
        if (need_albedo) {
            bool oldFlag = bsdf_context.spectrumed;
            bsdf_context.spectrumed = false;
            r.albedo = bsdf.energy(wi, bsdf_context);
            bsdf_context.spectrumed = oldFlag;
        }
        if (!continue_loop) return r;
        bsdf_context.rand = float3(sampler.next2f(g_buffer_heap), lobe_rand);
        auto sample_result = bsdf.sample(wi, bsdf_context, volume_stack);
        bool selected_wavelength_now =
            !spectrum_arg.selected_wavelength && bsdf_context.selected_wavelength;
        spectrum_arg.selected_wavelength = bsdf_context.selected_wavelength;
        if (selected_wavelength_now) {
            spectrum_arg.lambda = spectrum_arg.lambda[spectrum_arg.hero_index];
        }
        r.sample_flags = sample_result.throughput.flags;
        r.eta = sample_result.eta;
        if (!sample_result ||
            any(sample_result.throughput.val < 0.f) ||
            !all(is_finite(sample_result.throughput.val)) ||
            !is_finite(sample_result.pdf)) {
            continue_loop = false;
            beta = 0.0f;
            return r;
        }
        new_dir = basic_param.geometry.onb.to_world(sample_result.wo);
        float3 direct_shading_normal = r.normal;
        bool relocated_interaction = false;
        if constexpr (requires {
                          bsdf.sampled_interaction();
                      }) {
            auto sampled_interaction = bsdf.sampled_interaction();
            relocated_interaction = static_cast<bool>(sampled_interaction);
            if (relocated_interaction) {
                world_pos = sampled_interaction.position;
                plane_normal = sampled_interaction.normal;
                r.world_pos = world_pos;
                r.plane_normal = plane_normal;
                direct_shading_normal = plane_normal;
                contained_normal = false;
            }
        }
        if (!relocated_interaction) {
            bool output_outside =
                entering == mtl::is_reflective(sample_result.throughput.flags);
            float3 output_hemisphere = output_outside ? vertices_normal : -vertices_normal;
            new_dir = mtl::bend_to_hemisphere(new_dir, output_hemisphere);

            if (!basic_param.geometry.thin_walled &&
                mtl::is_transmissive(sample_result.throughput.flags)) {
                if (entering) {
                    mtl::Volume medium;
                    bool has_medium = fill_medium(
                        medium,
                        mtl::is_diffuse(sample_result.throughput.flags));
                    if (has_medium) {
                        medium.ior = bsdf_context.specular_fresnel.ior() / bsdf_context.inv_out_ior;
                        medium.boundary_id = boundary_id;
                        medium.nested_priority = basic_param.geometry.nested_priority;
                        if (!mtl::active_medium_insert(volume_stack, medium)) {
                            continue_loop = false;
                            beta = 0.0f;
                            return r;
                        }
                    }
                } else {
                    mtl::active_medium_remove(volume_stack, boundary_id);
                }
            }
        }
        if (selected_wavelength_now) {
            mtl::collapse_active_medium_wavelengths(volume_stack, spectrum_arg.hero_index);
        }

        pdf_bsdf = max(1e-4f, sample_result.pdf);
        beta *= sample_result.throughput.val / pdf_bsdf;

        if (mtl::is_delta(sample_result.throughput.flags)) pdf_bsdf = -1.0f;

        if (!relocated_interaction &&
            mtl::is_specular(sample_result.throughput.flags)) {
            di_use_specular = true;
        }
        if (!relocated_interaction &&
            mtl::is_transmissive(sample_result.throughput.flags)) {
            if (basic_param.geometry.thin_walled)
                r.new_ray_offset = -basic_param.geometry.onb.normal * basic_param.geometry.thickness;
        }
        if (relocated_interaction) {
            detail = max(detail, mtl::ShadingDetail::IndirectSpecular);
        } else if (mtl::is_non_delta(sample_result.throughput.flags) && sample_result.throughput.flags != mtl::BSDFFlags::SpecularTransmission) {
            detail = max(detail, mtl::is_specular(sample_result.throughput.flags) ? mtl::ShadingDetail::IndirectSpecular : mtl::ShadingDetail::IndirectDiffuse);
        }
        lighting::LightISResult is_result;
        ///////////// IS

        if (importance_sampling && pdf_bsdf > 1e-4f && contained_normal) {
            is_result = integrator::perform_light_importance_sampling_normal(
                resource_to_rec2020_mat,
                spectrum_arg,
                bsdf_eval_func,
                sampler,
                world_pos,
                plane_normal,
                new_dir,
                r.roughness,
                di_use_specular,
                !is_indirect_ray,
                mtl::make_normal_transform(inst_transform),
                vert_normals,
                vert_poses,
                hit,
                world_2_sky_mat,
                bdls_indices);
            di_result = is_result.radiance.xyz;
            di_dist = is_result.radiance.w;
        } else if (importance_sampling && pdf_bsdf > 1e-4f) {
            is_result = integrator::perform_light_importance_sampling(
                resource_to_rec2020_mat,
                spectrum_arg,
                bsdf_eval_func,
                sampler,
                world_pos,
                plane_normal,
                direct_shading_normal,
                new_dir,
                r.roughness,
                di_use_specular,
                !is_indirect_ray,
                world_2_sky_mat,
                bdls_indices);
            di_result = is_result.radiance.xyz;
            di_dist = is_result.radiance.w;
        } else {
            di_result = float3(0);
        }
        return r;
    });
}
