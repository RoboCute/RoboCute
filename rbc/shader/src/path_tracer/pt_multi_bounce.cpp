#ifndef OFFLINE_MODE
#define PT_CONFIDENCE_ACCUM
#endif
#define RBQ_USE_RAYQUERY
#define RBQ_USE_RAYQUERY_SHADOW

#include <path_tracer/integrator.hpp>
#include <path_tracer/pt_args.hpp>
#include <path_tracer/gbuffer.hpp>

#include <std/inplace_vector>
#include <volumetric/volume.hpp>

void accum_sky(SpectrumArg& spectrum_arg,
			   float3x3 world_2_sky_mat,
			   uint sky_heap_idx,
			   uint pdf_table_idx,
			   float pdf_bsdf,
			   float3 beta,
			   float3 sample_dir,
			   float3x3 resource_to_rec2020_mat,
			   float3& radiance) {
	if (sky_heap_idx == max_uint32) return;
	float2 uv = sampling::sphere_direction_to_uv(world_2_sky_mat, sample_dir);
	float theta;
	float phi;
	float3 wi;
	sampling::sphere_uv_to_direction_theta(world_2_sky_mat, uv, theta, phi, wi);
	float mis = 1.f;
	if (pdf_bsdf > 0.f) {
		auto tex_size = g_image_heap.uniform_idx_image_size(sky_heap_idx);
		auto sky_coord = int2(uv * float2(tex_size));
		sky_coord = clamp(sky_coord, int2(0), int2(tex_size) - 1);
		auto sky_pdf = g_buffer_heap.uniform_idx_buffer_read<float>(pdf_table_idx, sky_coord.y * tex_size.x + sky_coord.x);
		sky_pdf = _directional_pdf(sky_pdf, theta);
		mis = sampling::balanced_heuristic(pdf_bsdf, sky_pdf);
	}
	float3 sky_col = g_image_heap.uniform_idx_image_sample(sky_heap_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
	sky_col = resource_to_rec2020_mat * sky_col;
	sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);
	radiance += beta * mis * sky_col;
};
[[kernel_1d(128)]] int kernel(
	Buffer<MultiBouncePixel>& multi_bounce_pixel,
	Buffer<uint>& multi_bounce_pixel_counter,
#ifdef OFFLINE_MODE
	Buffer<GBuffer>& gbuffers,
#else
	Image<float>& radiance_img,
#endif
#ifndef OFFLINE_MODE
	Image<float>& confidence_img,
	Image<uint>& light_dirty_map,
#endif
	PTArgs args,
	uint2 size) {

	// return 0;
	auto id = dispatch_id().x;
	if (id >= multi_bounce_pixel_counter.read(0)) {
		return 0;
	}
	sampling::PCGSampler block_sampler(uint2(dispatch_id().x / 64u, args.frame_index));
	auto pixel = multi_bounce_pixel.read(id);
	uint2 coord(pixel.pixel_id >> 16u, pixel.pixel_id & 65535u);
	const uint MAX_DEPTH =
#ifdef OFFLINE_MODE
		args.bounce
#else
		4
#endif
		;
	uint depth = 0;
	float3 beta(pixel.beta[0], pixel.beta[1], pixel.beta[2]);
	auto origin_beta = beta;
	sampling::PCGSampler pcg_sampler(uint2(id, args.frame_index));
	float3 input_pos(pixel.input_pos);
	bool continue_loop = true;
	uint filter = Filter::POINT;
	float2 ddx(1.0f);
	float2 ddy(1.0f);
	float pdf_bsdf = pixel.pdf_bsdf;
	float3 new_dir(pixel.input_dir);
#ifdef OFFLINE_MODE
	uint buffer_id = (coord.x + coord.y * size.x);
	auto gb = gbuffers.read(buffer_id);
	float3 radiance(gb.radiance[0], gb.radiance[1], gb.radiance[2]);
#else
	float4 last = radiance_img.read(coord);
	float3 radiance = last.xyz;
#endif
	Ray ray(input_pos, new_dir, sampling::offset_ray_t_min);
#ifndef OFFLINE_MODE
	float confidence = 1.0f;
#endif
	vt::VTMeta vt_meta;
	vt_meta.frame_countdown = args.frame_countdown;

	mtl::ShadingDetail detail = mtl::ShadingDetail::IndirectDiffuse;
	std::inplace_vector<mtl::Volume, 0> volume_stack;
	SpectrumArg spectrum_arg;
	spectrum_arg.lambda = spectrum::sample_xyz(g_image_heap, pcg_sampler.next());
	spectrum_arg.selected_wavelength = true;
	spectrum_arg.hero_index = pcg_sampler.nextui() % 3;
	float3 last_beta = 0;
	float length_sum = pixel.length_sum;
	ProceduralGeometry procedural_geometry;
	bool reject = false;
	while (depth < MAX_DEPTH) {
		auto hit = rbq_trace_closest(ray, args, pcg_sampler, procedural_geometry, ONLY_OPAQUE_MASK);
		if (hit.miss()) {
			accum_sky(
				spectrum_arg,
				args.world_2_sky_mat,
				args.sky_heap_idx,
				args.pdf_table_idx,
				pdf_bsdf,
				beta,
				new_dir,
				args.resource_to_rec2020_mat,
				radiance);
#ifndef OFFLINE_MODE
			confidence *= args.sky_confidence;
#endif
			break;
		}
		last_beta = beta;
		float3 di_result;
		float di_dist;
		IntegratorResult result = sample_material(
			pcg_sampler,
			volume_stack,
			length_sum,
			hit,
			procedural_geometry,
			spectrum_arg,
			args.world_2_sky_mat,
			vt_meta,
			block_sampler.next(),
			input_pos,
			beta,
			continue_loop,
			false,
			filter,
			ddx,
			ddy,
#ifndef OFFLINE_MODE
			light_dirty_map,
#endif
			detail,
			args,
#ifndef OFFLINE_MODE
			confidence,
			args.sky_confidence,
#endif
			true,
			args.resource_to_rec2020_mat,
			di_result,
			di_dist,
			pdf_bsdf,
			new_dir,
			reject);
		radiance += di_result * last_beta;
#ifdef OFFLINE_MODE
		continue_loop &= (!reject);
		// see integrate_hashgrid_offline.cpp
		if (reject) {
			radiance = float3(-1000.0f);
		}
#endif
		if (!continue_loop) {
			break;
		}
		if (length_sum >= 0.0f) {
			length_sum += hit.ray_t;
		}
		///////////// Prepare next ray
		ray = Ray(result.new_ray_offset + sampling::offset_ray_origin(result.world_pos, dot(result.plane_normal, new_dir) < 0 ? -result.plane_normal : result.plane_normal), new_dir, 0.0f);

		input_pos = ray.origin();
		if (depth == (MAX_DEPTH - 1)) {
			if (!rbq_trace_any(ray, args, pcg_sampler, ONLY_OPAQUE_MASK)) {
				accum_sky(
					spectrum_arg,
					args.world_2_sky_mat,
					args.sky_heap_idx,
					args.pdf_table_idx,
					pdf_bsdf,
					beta,
					new_dir,
					args.resource_to_rec2020_mat,
					radiance);
#ifndef OFFLINE_MODE
				confidence *= args.sky_confidence;
#endif
			}
			break;
		}
		++depth;
	}
#ifdef OFFLINE_MODE
	gb.radiance = std::array<float, 3>(radiance.x, radiance.y, radiance.z);
	gbuffers.write(buffer_id, gb);
#else
	// if (any(origin_beta < 1e-4f && radiance > 1e-2f)) {
	// 	device_log("From PT: radiance {} beta {}", radiance, origin_beta);
	// }
	radiance_img.write(coord, float4(radiance, last.w));
	confidence_img.write(coord, float4(min(confidence_img.read(coord).x, confidence)));
#endif

	return 0;
}