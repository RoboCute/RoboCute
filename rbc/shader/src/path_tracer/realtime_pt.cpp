// #define USE_DLSS
#define DEBUG
#define RBC_USE_RAYQUERY
#define RBC_USE_RAYQUERY_SHADOW

#include <luisa/printer.hpp>
#define PT_CONFIDENCE_ACCUM
#define PT_SCREEN_RAY_DIFF
#define PT_MOTION_VECTORS

#define CACHE_PRIMARY_RAY_HIT

#ifndef USE_DLSS
#define STATIC_PRIMARY_RAY_RAND
#endif

#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/read_pixel.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/bayer.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <utils/onb.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <lighting/importance_sampling.hpp>
#include <path_tracer/pt_args.hpp>
#include <path_tracer/trace.hpp>
#include <surfel/accum_surfel.hpp>
#include <spectrum/spectrum.hpp>
#ifndef NO_RESTIR_DI
#include <restir/restirInitFunc.hpp>
#endif
#include <path_tracer/integrator.hpp>

#include <std/inplace_vector>
#include <volumetric/volume.hpp>
#include <volumetric/trace.hpp>

// #include <surfel/accum_surfel.hpp>

// Normal encoding variants ( match NormalEncoding )
#define NRD_NORMAL_ENCODING_RGBA8_UNORM 0
#define NRD_NORMAL_ENCODING_RGBA8_SNORM 1
#define NRD_NORMAL_ENCODING_R10G10B10A2_UNORM 2// supports material ID bits
#define NRD_NORMAL_ENCODING_RGBA16_UNORM 3
#define NRD_NORMAL_ENCODING_RGBA16_SNORM 4// also can be used with FP formats

// Roughness encoding variants ( match RoughnessEncoding )
#define NRD_ROUGHNESS_ENCODING_SQ_LINEAR 0	// linearRoughness * linearRoughness
#define NRD_ROUGHNESS_ENCODING_LINEAR 1		// linearRoughness
#define NRD_ROUGHNESS_ENCODING_SQRT_LINEAR 2// sqrt( linearRoughness )

#define NRD_NORMAL_ENCODING NRD_NORMAL_ENCODING_R10G10B10A2_UNORM
#define NRD_ROUGHNESS_ENCODING NRD_ROUGHNESS_ENCODING_LINEAR

using namespace luisa::shader;

float2 _NRD_EncodeUnitVector(float3 v, const bool bSigned) {
	v /= dot(abs(v), float3(1));

	float2 octWrap = (float2(1.0f) - abs(v.yx)) * (ite(v.xy >= 0.0f, float2(1.f), float2(0.f)) * 2.0f - 1.0f);
	v.xy = ite(v.z >= 0.0f, v.xy, octWrap);

	return ite(bSigned, v.xy, v.xy * 0.5f + 0.5f);
}

float4 NRD_FrontEnd_PackNormalAndRoughness(float3 N, float roughness, float materialID) {
	float4 p;

#if (NRD_ROUGHNESS_ENCODING == NRD_ROUGHNESS_ENCODING_SQRT_LINEAR)
	roughness = sqrt(saturate(roughness));
#elif (NRD_ROUGHNESS_ENCODING == NRD_ROUGHNESS_ENCODING_SQ_LINEAR)
	roughness *= roughness;
#endif

#if (NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_R10G10B10A2_UNORM)
	p.xy = _NRD_EncodeUnitVector(N, false);
	p.z = roughness;
	p.w = materialID;
#else
	// Best fit ( optional )
	N /= max(abs(N.x), max(abs(N.y), abs(N.z)));

#if (NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_RGBA8_UNORM || NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_RGBA16_UNORM)
	N = N * 0.5f + 0.5f;
#endif

	p.xyz = N;
	p.w = roughness;
#endif

	return p;
}

void accum_sky(SpectrumArg spectrum_arg,
			   float3x3 world_2_sky_mat,
			   uint sky_heap_idx,
			   uint pdf_table_idx,
			   float pdf_bsdf,
			   float sky_confidence,
			   float3 beta,
			   float3 sample_dir,
			   float& confidence,
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
	confidence *= lerp(1.0f, sky_confidence, mis);
	float3 sky_col = g_image_heap.uniform_idx_image_sample(sky_heap_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
	sky_col = resource_to_rec2020_mat * sky_col;

	sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);
	radiance += beta * mis * sky_col;
};

uint4 pack_triangle_hit(CommittedHit hit) {
	if (!hit.hit_triangle()) {
		hit.bary = float2(-100.f);
	}
	return uint4(hit.inst, hit.prim, bit_cast<uint>(hit.bary.x), bit_cast<uint>(hit.bary.y));
}

CommittedHit unpack_triangle_hit(uint4 v) {
	CommittedHit hit;
	hit.inst = v.x;
	hit.prim = v.y;
	hit.bary.x = bit_cast<float>(v.z);
	hit.bary.y = bit_cast<float>(v.w);
	if (any(hit.bary < 0.f)) {
		hit.bary = 0.f;
		hit.hit_type = HitTypes::HitProcedural;
	}
	return hit;
}

// using PackedVector2 = std::array<float, 2>;
// using PackedVector3 = std::array<float, 3>;
// struct GBuffer {
// 	PackedVector3 diffuse_radiance; // demodulated
// 	PackedVector3 specular_radiance;// demodulated
// 	float depth;
// 	PackedVector3 normal;
// 	float roughness;
// 	PackedVector3 gi_reflect_dir;
// 	float gi_reflect_dist;
// 	PackedVector3 di_reflect_dir;
// 	float di_reflect_dist;
// 	PackedVector2 motion_vec;	   // [0, 1] uv space motion_vectors
// };

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& emission_img,
	Image<float>& depth_img,
	Image<float>& mv_img,
	// Image<float>& hitpos_img,
	Image<float>& radiance_img,
#ifdef USE_DLSS
	Image<float>& specular_dist_img,
	Image<float>& hitmv_img,
#endif
	Image<float>& specular_albedo_img,
	Image<float>& albedo_img,
	Image<float>& beta_img,
	Image<float>& normal_roughness_img,
	Image<uint>& sample_flags_img,
	Image<float>& di_img,
#ifndef NO_RESTIR_DI
	Image<uint>& ReservoirCurr,
#endif

#ifdef CACHE_PRIMARY_RAY_HIT
	Image<uint>& vbuffer,
	uint sample,
	uint spp,
#endif
	Image<float>& confidence_map,
	Image<uint>& light_dirty_map,

	Buffer<uint>& surfel_key_buffer,
	Buffer<uint>& surfel_value_buffer,
	Image<uint>& surfel_mark,

	Buffer<MultiBouncePixel>& multi_bounce_pixel,
	Buffer<uint>& multi_bounce_pixel_counter,
	Buffer<float4x4>& last_transform_buffer,
	PTArgs args,
	uint2 size) {

	auto coord = dispatch_id().xy;
	auto screen_uv = (float2(coord) + args.jitter_offset + float2(0.5)) / float2(size);
	float3 dir;
	Ray ray;
	float3 up_ray_dir;
	float3 right_ray_dir;
	/////////////// RR
	SharedArray<float, 16 * 8> quad_beta;
	SharedArray<uint, 8 * 4> quad_flags;
	auto quad_id = thread_id().xy;
	quad_id /= 2u;
	auto quad_group_id = (quad_id.y * 8 + quad_id.x) * 4u;
	auto quad_local_id = (thread_id().x & 1u) + (thread_id().y & 1u) * 2;
	quad_beta[quad_group_id + quad_local_id] = 0;
	if (quad_local_id == 0) {
		quad_flags[quad_group_id / 4] = 0;
	}
	if (any(coord >= size)) {
		return 0;
	}
	/////////////// RR
	sampling::HeitzSobol sampler(coord, args.frame_index);
#ifdef CACHE_PRIMARY_RAY_HIT
	sampler.dimension = sample * 3;
#endif
	sampling::PCGSamplerOffsetted pcg_sampler(uint3(dispatch_id().xy, args.frame_index));
	pcg_sampler.offset = sampler.next3f(g_buffer_heap);

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
		// up pixel
		proj = float4(((screen_uv + (float2(0, 0.5f) / float2(size))) * 2.f - 1.0f), 0.f, 1);
		world_pos = args.inv_vp * proj;
		world_pos /= world_pos.w;
		up_ray_dir = normalize(world_pos.xyz - args.cam_pos);
		// right pixel
		proj = float4(((screen_uv + (float2(0.5f, 0) / float2(size))) * 2.f - 1.0f), 0.f, 1);
		world_pos = args.inv_vp * proj;
		world_pos /= world_pos.w;
		right_ray_dir = normalize(world_pos.xyz - args.cam_pos);

		// if (args.enable_physical_camera) {
		// 	auto coord_lens = sampling::sample_uniform_disk_concentric(pcg_sampler.next2f()) * args.lens_radius;
		// 	auto p_lens = float3(coord_lens, 0.f);
		// 	float3 dst_pos = near_world_pos.xyz + dir * args.focus_distance;
		// 	float4 p_lens_r = args.inv_view * float4(p_lens, 1.0f);
		// 	p_lens_r /= p_lens_r.w;
		// 	near_world_pos = p_lens_r;
		// 	dir = normalize(dst_pos - near_world_pos.xyz);
		// 	right_ray_dir = dir;
		// 	up_ray_dir = dir;
		// }
		ray = Ray(near_world_pos.xyz, dir, sampling::offset_ray_t_min, dir_len);
	}
#ifdef STATIC_PRIMARY_RAY_RAND
	sampling::BayerSampler parimary_ray_sampler(dispatch_id().xy);
#else
#define parimary_ray_sampler sampler
#endif

#ifdef USE_DLSS
#define ray_sampler pcg_sampler
#else
#define ray_sampler sampler
#endif

	ProceduralGeometry procedural_geometry;
#ifdef CACHE_PRIMARY_RAY_HIT
	bool inject_gbuffer = sample == 0;
	CommittedHit hit;
	if (inject_gbuffer) {
		hit = rbc_trace_closest(ray, args, parimary_ray_sampler, procedural_geometry);
		if (spp > 1)
			vbuffer.write(coord, pack_triangle_hit(hit));
	} else {
		hit = unpack_triangle_hit(vbuffer.read(coord));
		if (hit.hit_procedural()) {
			commit_procedural(ray, hit, sampler, hit.ray_t, procedural_geometry);
		}
	}
#else
	auto hit = rbc_trace_closest(ray, args, parimary_ray_sampler, procedural_geometry);
#endif
	float3 addition_color{0};
	float3 albedo{1};
	float3 specular_albedo{0};
	float4 normal_rough = float4(0, 0, 1, 0);
	float hit_dist;
	float4 hitpos;// xyz: pos, w: hit normal
	float3 hit_normal;
	float3 radiance(0.0f);
	bool hit_sky = false;
	float confidence = 1.0f;
	float3 mv;
	float3 first_plane_normal = float3(0, 0, 1);
	float3 first_new_dir;
	float depth_value = args.default_depth;
	float3 prim_worldpos;
	mtl::BSDFFlags sample_flags = mtl::BSDFFlags::None;
	const int MAX_DEPTH = 2;
	float3 beta(1.f);
	float4 first_di_result{0};
	float3 primary_beta{1.f};

#ifdef USE_DLSS
	float2 hit_mv;
	float specular_hit_dist;
#endif
	auto write_tex = [&](float alpha_holder) {
#ifdef CACHE_PRIMARY_RAY_HIT
		float rate = 1.0f / float(spp);
		first_di_result *= rate;

#ifdef USE_DLSS
		specular_hit_dist = hit_dist;
		if (is_diffuse(sample_flags)) {
			specular_hit_dist = 0.0f;
		}
		specular_hit_dist *= rate;
		hit_mv *= rate;
#endif//USE_DLSS
		if (inject_gbuffer) {
#endif
			albedo_img.write(coord, float4(albedo, alpha_holder));
			beta_img.write(coord, float4(sqrt(primary_beta), alpha_holder));
			depth_img.write(coord, float4(depth_value));
			normal_rough = NRD_FrontEnd_PackNormalAndRoughness(normal_rough.xyz, normal_rough.w, ite(hit_sky, 1.0f, 0.f));
			normal_roughness_img.write(coord, normal_rough);
			// hitpos_img.write(coord, hitpos);
			emission_img.write(coord, float4(addition_color, 1.0f));
			mv_img.write(coord, float4(mv, 1.f));
			// radiance_img.write(coord, float4(radiance, 1.0f));
#ifdef CACHE_PRIMARY_RAY_HIT
		} else {
			confidence = min(confidence_map.read(coord).x, confidence);
			first_di_result += di_img.read(coord);
#ifdef USE_DLSS
			specular_hit_dist += specular_dist_img.read(coord).x;
			hit_mv += hitmv_img.read(coord).xy;
#endif//USE_DLSS
		}
#endif// CACHE_PRIMARY_RAY_HIT
		radiance_img.write(coord, float4(radiance, hit_dist));
		confidence_map.write(coord, float4(confidence));
		di_img.write(coord, first_di_result);
		sample_flags_img.write(coord, uint4((uint)sample_flags));
		specular_albedo_img.write(coord, float4(specular_albedo, 1.0f));
#ifdef USE_DLSS
		bool last_spp = true;
#ifdef CACHE_PRIMARY_RAY_HIT
		last_spp = sample == (spp - 1);
#endif
		specular_dist_img.write(coord, float4(specular_hit_dist));
		hitmv_img.write(coord, float4(hit_mv, 0.0f, 0.0f));
#endif//USE_DLSS
	};

	std::inplace_vector<mtl::Volume, mtl::Volume::MAX_VOLUME_STACK_SIZE> volume_stack;
	SpectrumArg spectrum_arg;
	spectrum_arg.lambda = spectrum::sample_xyz(g_image_heap, fract(sampler.next(g_buffer_heap) + pcg_sampler.next() / 255.f));
	spectrum_arg.hero_index = pcg_sampler.nextui() % 3;
	if (hit.miss()) {
		confidence = 0.0f;
		if (args.sky_heap_idx != max_uint32) {
			addition_color = g_image_heap.uniform_idx_image_sample(args.sky_heap_idx, sampling::sphere_direction_to_uv(args.world_2_sky_mat, dir), Filter::LINEAR_POINT, Address::EDGE).xyz;
			addition_color = args.resource_to_rec2020_mat * addition_color;
		}
		write_tex(0.f);
		surfel_mark.write(coord, uint4(max_uint32));
		return 0;
	}
	float3 input_pos = ray.origin();
	float pdf_bsdf = -1.0f;
	float2 ddx = float2(0);
	float2 ddy = float2(0);
	uint filter = Filter::ANISOTROPIC;
	float3 new_dir = dir;
	bool continue_loop = true;
	int depth = 0;
	int transparent_depth = 0;
	const int TRANS_MAX_DEPTH = 4;
	bool write_gbuffer = false;
	mtl::ShadingDetail detail = mtl::ShadingDetail::Default;
	vt::VTMeta vt_meta;
	vt_meta.frame_countdown = args.frame_countdown;
	float3 last_beta(0.f);
	bool hitted_non_delta = false;
	float length_sum = 0.0f;
	// sampling::PCGSampler block_sampler(uint3(dispatch_id().xy / 8u, args.frame_index));
	while (depth < MAX_DEPTH) {
		float3 di_result;
		float di_dist;
		float lobe_rand;
		last_beta = beta;
		bool require_last_local_pos = (inject_gbuffer && !write_gbuffer);
#ifdef USE_DLSS
		require_last_local_pos |= (write_gbuffer && depth == 1);
#endif
		// if (write_gbuffer) {
		// 	lobe_rand = block_sampler.next();
		// } else
		{
			lobe_rand = sampler.next(g_buffer_heap);
		}
		bool selected_wavelength = spectrum_arg.selected_wavelength;
		bool reject;
		IntegratorResult result = sample_material(
			pcg_sampler,
			volume_stack,
			length_sum,
			hit,
			procedural_geometry,
			spectrum_arg,
			args.world_2_sky_mat,
			vt_meta,
			lobe_rand,
			input_pos,
			beta,
			continue_loop,
			!hitted_non_delta,
			filter,
			args.tex_grad_scale,
			ddx,
			ddy,
			up_ray_dir,
			right_ray_dir,
			light_dirty_map,
			detail,
			args,
			confidence,
			args.sky_confidence,
#ifdef NO_RESTIR_DI
			true,
#else
			write_gbuffer,
#endif
			args.resource_to_rec2020_mat,
			di_result,
			di_dist,
			pdf_bsdf,
			new_dir,
			require_last_local_pos,
			reject);
		if (length_sum >= 0.0f) {
			length_sum += hit.ray_t;
		}
		if (!selected_wavelength && spectrum_arg.selected_wavelength) {
			spectrum_arg.selected_wavelength = true;
			spectrum::modify_throughput(g_image_heap, spectrum_arg, beta, last_beta, di_result);
		}
		if (is_transmissive(result.sample_flags) && is_non_diffuse(result.sample_flags)) {
			if (transparent_depth < TRANS_MAX_DEPTH) {
				depth -= 1;
				++transparent_depth;
			}
		}

		/////////// primary ray
		if (!write_gbuffer) {
			write_gbuffer = true;
			hitted_non_delta = is_non_delta(result.sample_flags);
			first_di_result += float4(di_result * last_beta, di_dist);
			///////////// Record gbuffer in primary ray
			specular_albedo += result.specular_albedo;
			albedo *= result.albedo;
			addition_color += result.emission;// TODO: maybe noisy throw glass
											  ///// Gbuffer
#ifdef CACHE_PRIMARY_RAY_HIT
			if (inject_gbuffer && hit.hit_triangle()) {
#endif
				auto last_world_pos = (last_transform_buffer.read(result.user_id) * float4(result.last_local_pos, 1));
				mv = result.world_pos.xyz - last_world_pos.xyz;
#ifdef CACHE_PRIMARY_RAY_HIT
			}
#endif
			normal_rough = float4(result.normal, result.roughness);
			auto view_pos = args.view * float4(result.world_pos, 1.f);
			depth_value = abs(view_pos.z / view_pos.w);

			sample_flags = result.sample_flags;
			first_plane_normal = result.plane_normal;
			prim_worldpos = result.world_pos;
			first_new_dir = new_dir;

		} else {
			if (!hitted_non_delta && is_non_delta(result.sample_flags)) {
				hitted_non_delta = true;
				auto sum = result.specular_albedo + result.albedo;
				specular_albedo = max(float3(denoise_min_albedo), specular_albedo * sum);
				normal_rough = float4(result.normal, result.roughness);
			}
			if (depth <= 0) {
				first_di_result += float4(di_result * last_beta, di_dist);
				first_di_result.xyz += result.emission * last_beta;
			} else if (depth == 1) {
				first_di_result.xyz += di_result * last_beta;
				first_di_result.xyz += result.emission * last_beta;
				primary_beta = max(float3(1e-4f), last_beta);
				beta /= primary_beta;
				last_beta /= primary_beta;
				auto encoded_normal = _NRD_EncodeUnitVector(result.plane_normal, false);
				hit_normal = result.plane_normal;
				hitpos = float4(result.world_pos, bit_cast<float>((uint(encoded_normal.x * float(0xffff)) & 0xffff) + (uint(encoded_normal.y * float(0xffff)) << 16)));

				hit_dist = hit.ray_t;
#ifdef USE_DLSS
				if (hit.hit_triangle()) {
					float4 curr_proj = args.vp * float4(hitpos.xyz, 1.0f);
					float2 nonjitter_uv = (curr_proj.xy / curr_proj.w) * 0.5f + 0.5f;
					auto last_world_pos = (last_transform_buffer.read(result.user_id) * float4(result.last_local_pos, 1));
					auto last_proj = args.last_vp * last_world_pos;
					float2 last_uv = (last_proj.xy / last_proj.w) * 0.5f + 0.5f;
					hit_mv = clamp(nonjitter_uv - last_uv, -100.0f, 100.0f);
				}
#endif
				// albedo demodulated, require multiply albedo manually
			} else {
				radiance += di_result * last_beta;
				radiance += result.emission * last_beta;
			}
		}

		if (!continue_loop) {
			last_beta = float3(0.f);
			beta = float3(0.f);
			break;
		}

		///////////// Prepare next ray
		ray = Ray(result.new_ray_offset + sampling::offset_ray_origin(result.world_pos, dot(result.plane_normal, new_dir) < 0 ? -result.plane_normal : result.plane_normal), new_dir, sampling::offset_ray_t_min);

		if (depth < int(MAX_DEPTH - 1)) {
			hit = mtl::trace_volumetric(volume_stack, beta, detail, ray, args, ray_sampler, procedural_geometry, depth < 0 ? 255u : ONLY_OPAQUE_MASK);

			if (hit.miss()) {
				if (depth == 0) {
					hitpos = float4(new_dir, 0);
					hit_dist = 16384.f;
					hit_sky = true;
				}
				accum_sky(
					spectrum_arg,
					args.world_2_sky_mat,
					args.sky_heap_idx,
					args.pdf_table_idx,
					pdf_bsdf,
					args.sky_confidence,
					beta,
					new_dir,
					confidence,
					args.resource_to_rec2020_mat,
					radiance);
				last_beta = float3(0);
				beta = float3(0);
				break;
			}
		} else {
			break;
		}
		input_pos = ray.origin();
		++depth;
	}
	if (depth < 0 && transparent_depth < TRANS_MAX_DEPTH) {
		if (args.sky_heap_idx != max_uint32 && reduce_sum(beta) > 1e-5f) {
			float3 sky_col = g_image_heap.uniform_idx_image_sample(args.sky_heap_idx,
																   sampling::sphere_direction_to_uv(args.world_2_sky_mat, new_dir), Filter::LINEAR_POINT, Address::EDGE)
								 .xyz;
			sky_col = args.resource_to_rec2020_mat * sky_col;
			sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);
			radiance += beta * sky_col;
		}
		// addition_color = float3(0.5f);
		last_beta = float3(0);
		beta = float3(0);
		hit_sky = true;
	}

	/*
	after test: roughness to maximum ggx pdf is about:
	roughness 1 with max pdf 0.1013207882642746
	roughness 0.9 with max pdf 0.15442828834056854
	roughness 0.8 with max pdf 0.24736349284648895
	roughness 0.7 with max pdf 0.4219903349876404
	roughness 0.6 with max pdf 0.7817889451980591
	roughness 0.5 with max pdf 1.6211143732070923
	roughness 0.4 with max pdf 3.9577794075012207
	roughness 0.3 with max pdf 12.508867263793945
	roughness 0.2 with max pdf 63.32357406616211
	*/

	if (!any(is_finite(radiance))) {
		radiance = float3(0.f);
	}
	// prepar RR data
	sync_block();
	// from Physically Based Shader Design in Arnold, Langlands, 2014
	float rr_scale = reduce_sum(beta);
	quad_beta[quad_group_id + quad_local_id] = rr_scale;
	sync_block();
	if (quad_flags.atomic_fetch_add(quad_group_id / 4, 1) == 0) {
		float sum = 0.f;
		for (int i = 0; i < 4; ++i) {
			sum += quad_beta[quad_group_id + i];
		}
		if (sum > 1e-5f) {
			float rand = pcg_sampler.next();
			int i = 0;
			while (i < 4) {
				float num = quad_beta[quad_group_id + i] / sum;
				if (rand <= num) {
					quad_beta[quad_group_id + i] = sum;
					++i;
					break;
				} else {
					rand -= num;
					quad_beta[quad_group_id + i] = -1.0f;
				}
				++i;
			}
			while (i < 4) {
				quad_beta[quad_group_id + i] = -1.0f;
				++i;
			}
		} else {
			for (int i = 0; i < 4; ++i) {
				quad_beta[quad_group_id + i] = -1.0f;
			}
		}
	}
	sync_block();
	if (rr_scale > 1e-5f && quad_beta[quad_group_id + quad_local_id] > 1e-8f) {
		MultiBouncePixel pixel;
		pixel.pixel_id = ((coord.x << 16u) | coord.y);
		auto beta_scale = quad_beta[quad_group_id + quad_local_id] / max(1e-5f, rr_scale);
		beta *= beta_scale;
		pixel.beta[0] = beta.x;
		pixel.beta[1] = beta.y;
		pixel.beta[2] = beta.z;
		pixel.input_pos[0] = ray._origin[0];
		pixel.input_pos[1] = ray._origin[1];
		pixel.input_pos[2] = ray._origin[2];
		pixel.pdf_bsdf = pdf_bsdf;
		pixel.input_dir[0] = ray._dir[0];
		pixel.input_dir[1] = ray._dir[1];
		pixel.input_dir[2] = ray._dir[2];
		pixel.length_sum = length_sum;
		auto index = multi_bounce_pixel_counter.atomic_fetch_add(0, 1);
		multi_bounce_pixel.write(index, pixel);
	}
	accum_surfel(
		coord,
		prim_worldpos,
		normal_rough.w,
		hit_sky,
		sample_flags,
		hitpos.xyz,
		hit_normal,
		surfel_key_buffer,
		surfel_value_buffer,
		surfel_mark,
		pcg_sampler,
		confidence,
		args.cam_pos,
		args.grid_size,
		args.buffer_size,
		args.max_offset,
		args.frame_index);
#ifndef NO_RESTIR_DI
	RestirInit(g_accel, prim_worldpos, first_plane_normal, normal_rough.xyz, first_new_dir, normal_rough.w, sample_flags, ReservoirCurr, g_heap, pcg_sampler, args.cam_pos, args.inv_vp, args.light_count, args.mesh_lights_heap_idx, args.bvh_heap_idx, args.point_lights_heap_idx, args.spot_lights_heap_idx, args.area_lights_heap_idx, args.inst_buffer_heap_idx, args.mat_idx_buffer_heap_idx, args.colorspace_matrix, args.world_2_sky_mat, args.sky_lum_idx, args.alias_table_idx, args.pdf_table_idx);
#endif
	write_tex(1.f);
	return 0;
}