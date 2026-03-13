
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/gbuffer.hpp>
#include <path_tracer/pt_args.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <utils/onb.hpp>
#include <path_tracer/trace.hpp>

using namespace luisa::shader;

// Decode normal from packed uint format used in GBuffer
inline float3 decode_packed_normal(float packed_val) {
    uint packed = bit_cast<uint>(packed_val);
    float2 encoded;
    encoded.x = float(packed & 0xffff) * (1.0f / 65535.0f);
    encoded.y = float(packed >> 16) * (1.0f / 65535.0f);
    return sampling::decode_unit_vector(encoded);
}

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &out_img,
    PTArgs args,
    float ray_radius,
    float pow_atten,
    bool use_cosine_sample) {

    auto coord = dispatch_id().xy;
    auto size = dispatch_size().xy;
    sampling::HeitzSobol sampler(coord, args.frame_index);
    sampling::PCGSamplerOffsetted pcg_sampler(uint3(dispatch_id().xy, args.frame_index));
    pcg_sampler.offset = sampler.next3f(g_buffer_heap) / 128.f;
    auto screen_uv = (float2(coord) + sampling::sample_uniform_disk_concentric(pcg_sampler.next2f()) + 0.5f) / float2(size);
    // Camera primary ray
    float3 dir;
    Ray ray;
    {
        auto proj = float4((screen_uv * 2.f - 1.0f), 0.f, 1);
        auto world_pos = args.inv_vp * proj;
        world_pos /= world_pos.w;
        auto n_dir = world_pos.xyz - args.cam_pos;
        auto dir_len = length(n_dir);
        dir = n_dir / max(1e-4f, dir_len);
        proj.z = 1.0f;
        auto near_world_pos = args.inv_vp * proj;
        near_world_pos /= near_world_pos.w;
        if (args.enable_physical_camera) {
            auto coord_lens = sampling::sample_uniform_disk_concentric(pcg_sampler.next2f()) * args.lens_radius;
            auto p_lens = float3(coord_lens, 0.f);
            float3 dst_pos = near_world_pos.xyz + dir * args.focus_distance;
            float4 p_lens_r = args.inv_view * float4(p_lens, 1.0f);
            p_lens_r /= p_lens_r.w;
            near_world_pos = p_lens_r;
            dir = normalize(dst_pos - near_world_pos.xyz);
        }
        ray = Ray(near_world_pos.xyz, dir, sampling::offset_ray_t_min, dir_len);
    }
    auto write_tex = [&](float ao) {
        float alpha = 1.f;
        float3 ao_val(ao);
        if (!args.reset_emission) {
            auto old_val = float4(out_img.read(coord));
            ao_val += old_val.xyz;
            alpha += old_val.w;
        }
        out_img.write(coord, float4(ao_val, 1));
    };
    ProceduralGeometry procedural_geometry;
    auto hit = rbc_trace_closest(ray, args, sampler, procedural_geometry);
    float3 geometry_normal;
    float3 geometry_pos;
    if (hit.hit_triangle()) {
        // Get instance info and transform
        auto user_id = g_accel.instance_user_id(hit.inst);
        auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
        auto inst_transform = g_accel.instance_transform(hit.inst);

        // Read vertex positions and normals
        bool contained_normal = false;
        bool contained_tangent = false;
        uint contained_uv = 0;
        geometry::Triangle triangle;
        auto vertices = geometry::read_vertices(g_buffer_heap, hit.prim, inst_info.mesh, contained_normal, contained_tangent, contained_uv, triangle);

        // Interpolate position and normal in local space
        auto local_pos = hit.interpolate(vertices[0].pos, vertices[1].pos, vertices[2].pos);

        if (contained_normal) {
            auto local_normal = hit.interpolate(vertices[0].normal, vertices[1].normal, vertices[2].normal);
            // Transform normal to world space (using w=0 for vectors)
            geometry_normal = normalize((inst_transform * float4(local_normal, 0.0f)).xyz);
        } else {
            // Compute geometric normal from triangle vertices
            auto e1 = vertices[1].pos - vertices[0].pos;
            auto e2 = vertices[2].pos - vertices[0].pos;
            auto local_normal = normalize(cross(e1, e2));
            geometry_normal = normalize((inst_transform * float4(local_normal, 0.0f)).xyz);
        }
        if (dot(geometry_normal, ray.dir()) >= 0) {
            geometry_normal = -geometry_normal;
        }

        // Transform position to world space
        auto world_pos_h = inst_transform * float4(local_pos, 1.0f);
        geometry_pos = world_pos_h.xyz / world_pos_h.w;
    } else {
        write_tex(0);
        return 0;
    }

    float3 local_dir;
    if (use_cosine_sample) {
        local_dir = sampling::cosine_sample_hemisphere(pcg_sampler.next2f());
    } else {
        local_dir = sampling::uniform_sample_hemisphere(pcg_sampler.next2f());
    }
    mtl::Onb onb(geometry_normal);
    auto sample_dir = onb.to_world(local_dir);
    ray.set_origin(sampling::offset_ray_origin(geometry_pos, geometry_normal));
    ray.set_dir(sample_dir);
    ray.t_max = ray_radius;
    hit = rbc_trace_closest(ray, args, sampler, procedural_geometry);
    float ao = 1;
    if (hit.hit_triangle()) {
        ao = hit.ray_t / ray_radius;
    }
    ao = pow(ao, pow_atten);
    write_tex(ao);
    return 0;
}
