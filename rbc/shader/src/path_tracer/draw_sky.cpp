#include <luisa/std.hpp>
#include <luisa/resources/texture.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/pcg.hpp>
#include <spectrum/spectrum.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> _emission,
    BindlessImage &image_heap,
    BindlessVolume &volume_heap,
    uint sky_idx,
    float3x3 resource_to_rec2020_mat,
    float3x3 world_2_sky_mat,
    float4x4 inv_vp,
    float3 cam_pos,
    float2 jitter,
    uint frame_index) {
    sampling::PCGSampler sampler(uint3(dispatch_id().xy, frame_index));
	SpectrumArg spectrum_arg;
	spectrum_arg.lambda = spectrum::sample_xyz(image_heap, fract(sampler.next() + sampler.next() / 255.f));
	spectrum_arg.hero_index = sampler.nextui() % 3;

    auto coord = dispatch_id().xy;
    auto size = dispatch_size().xy;
    auto uv = (float2(coord) + jitter + float2(0.5)) / float2(size);
    auto proj = float4((uv * 2.f - 1.0f), 0.f, 1);
    auto world_pos = inv_vp * proj;
    world_pos /= world_pos.w;
    auto dir = normalize(world_pos.xyz - cam_pos);
    float4 col(1);
    if (sky_idx != max_uint32) {
    	col = image_heap.uniform_idx_image_sample(sky_idx, sampling::sphere_direction_to_uv(world_2_sky_mat, dir), Filter::LINEAR_POINT, Address::EDGE);
    }
    col.xyz = resource_to_rec2020_mat * col.xyz;
	col.xyz = spectrum::emission_to_spectrum(image_heap, volume_heap, spectrum_arg, col.xyz);
    _emission.write(coord, col);
    return 0;
}