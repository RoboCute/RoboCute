#include <luisa/std.hpp>
#include <material/mats.hpp>
#include <sampling/sample_funcs.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/image_heap_extern.hpp>
struct RasterBasicParameter {
    mtl::openpbr::Parameter::Geometry geometry;
    mtl::openpbr::Parameter::Emission emission;
    mtl::openpbr::Parameter::Base base;
};

[[kernel_2d(16, 8)]] int kernel(
    Image<uint> &id_img,
    Image<float> &img,
    Buffer<float4x4> &transform_buffer,
    float4x4 inv_vp,
    float3x3 resource_to_rec2020_mat,
    float3x3 world_2_sky_mat,
    float3 cam_pos,
    uint sky_heap_idx,
    uint frame_countdown,
    float3 light_dir,
    float3 light_color) {
    auto coord = dispatch_id().xy;
    auto id = id_img.read(coord);
    auto accum_sky = [&](float3 new_dir) {
        if (sky_heap_idx != max_uint32) {
            float2 uv = sampling::sphere_direction_to_uv(world_2_sky_mat, new_dir);
            float3 sky_col = g_image_heap.uniform_idx_image_sample(sky_heap_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
            sky_col = resource_to_rec2020_mat * sky_col;
            return sky_col;
        }
        return float3(0.5f);
    };
    auto proj = (float2(coord) + 0.5f) / float2(dispatch_size().xy) * 2.f - 1.f;
    auto dst_pos = inv_vp * float4(proj, 0.5f, 1.0f);
    dst_pos /= dst_pos.w;
    auto ray_dir = normalize(dst_pos.xyz - cam_pos);

    if (any(id.xy == max_uint32)) {
        img.write(coord, float4(accum_sky(ray_dir), 1));
        return 0;
    }
    auto user_id = id.x;
    auto prim_id = id.y;
    auto bary = bit_cast<float2>(id.zw);
    auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
    geometry::Triangle triangle;
    bool contained_normal;
    uint contained_uv;
    bool contained_tangent;
    auto vertices = geometry::read_vertices(g_buffer_heap, prim_id, inst_info.mesh, contained_normal, contained_tangent, contained_uv, triangle);
    auto mat_meta = material::mat_meta(g_buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, prim_id);
    auto inst_transform = transform_buffer.read(user_id);
    auto local_pos = interpolate(bary, vertices[0].pos, vertices[1].pos, vertices[2].pos);
    std::array<float3, 3> vert_poses;
    std::array<float3, 3> vert_normals;
    float3 world_pos;
    float2 uv;
    float3 vertices_normal;
    float3 plane_normal;
    for (int i = 0; i < 3; ++i) {
        vert_poses[i] = (inst_transform * float4(vertices[i].pos, 1)).xyz;
        vert_normals[i] = vertices[i].normal;
    }
    plane_normal = cross(vert_poses[0] - vert_poses[1], vert_poses[0] - vert_poses[2]);
    plane_normal = normalize(plane_normal);
    world_pos = (inst_transform * float4(local_pos, 1)).xyz;
    uv = interpolate(bary, vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);

    if (contained_normal) {
        vertices_normal = interpolate(bary, vertices[0].normal, vertices[1].normal, vertices[2].normal);
        vertices_normal = normalize((inst_transform * float4(vertices_normal, 0)).xyz);
        if (dot(ray_dir, plane_normal) * dot(ray_dir, vertices_normal) < 0.0f) {
            vertices_normal = plane_normal;
        }
    } else {
        vertices_normal = plane_normal;
    }
    RasterBasicParameter basic_param;

    if (contained_tangent) {
        auto tangent = interpolate(bary, vertices[0].tangent, vertices[1].tangent, vertices[2].tangent);
        basic_param.geometry.onb.tangent = normalize(inst_transform * float4(tangent.xyz, 0)).xyz;
        basic_param.geometry.onb.bitangent = normalize(cross(vertices_normal, tangent.xyz));
        basic_param.geometry.onb.normal = vertices_normal;
    } else {
        basic_param.geometry.onb = mtl::Onb(vertices_normal);
    }

    mtl::Onb vertices_onb = basic_param.geometry.onb;

    vt::VTMeta vt_meta;
    vt_meta.frame_countdown = frame_countdown;
    uint texture_filter = Filter::ANISOTROPIC;
    bool reject;
    transform_to_params(
        g_buffer_heap,
        g_image_heap,
        mat_meta,
        basic_param,
        texture_filter,
        vt_meta,
        uv,
        float4(0),// float4(ddx, ddy),
        ray_dir,
        world_pos,
        reject);
    if (basic_param.geometry.thin_walled && dot(ray_dir, vertices_normal) >= 0.0f) {
        basic_param.geometry.onb.normal = -basic_param.geometry.onb.normal;
    }

    float3 diffuse_color = light_color * dot(basic_param.geometry.onb.normal, light_dir) * basic_param.base.color.origin();
    float3 color = diffuse_color + basic_param.emission.luminance.origin();
    img.write(coord, float4(color, 1));
    return 0;
}