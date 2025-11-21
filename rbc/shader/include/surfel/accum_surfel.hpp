#pragma once
#include <luisa/printer.hpp>
#include <surfel/surfel_grid.hpp>
#include <sampling/sample_funcs.hpp>
#include <utils/onb.hpp>
#include <bsdfs/base/bsdf_flags.hpp>
using namespace luisa::shader;

template<typename Sampler>
static void accum_surfel(
	uint2 id,
	float3 primary_worldpos,
	float rough,
	bool hitsky,
	mtl::BSDFFlags sample_flags,
	float3 hitpos,
	float3 hit_normal,
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	Image<uint>& surfel_mark,
	Sampler& sampler,
	float confidence,
	float3 cam_pos,
	float grid_size,
	uint buffer_size,
	uint max_offset,
	uint frame_index) {
	// hit sky or roughness < 0.5
	if (hitsky) {
		surfel_mark.write(id, uint4(max_uint32));
		return;
	}
	if (is_diffuse(sample_flags)) {
		rough = 1.0f;
	}
	uint mark_result = max_uint32;
	mtl::Onb onb(hit_normal);
	float3 sample_normal = normalize(onb.to_world(sampling::uniform_sample_cone(sampler.next2f(), 0.9f)));

	float3 abs_normal = abs(sample_normal);
	uint face = 0;
	if (abs_normal.y > abs_normal.z && abs_normal.y > abs_normal.x) {
		face = ite(sample_normal.y < 0, 4, 1);
	} else if (abs_normal.z > abs_normal.y && abs_normal.z > abs_normal.x) {
		face = ite(sample_normal.z < 0, 5, 2);
	} else {
		face = ite(sample_normal.x < 0, 3, 0);
	}

	uint key;
	uint key_slot;
	hitpos.xyz += hit_normal / (grid_size * 2.0f);
	int level;
	if (get_grid_level(cam_pos, grid_size, hitpos.xyz, level)) {
		// input jitter
		rough = saturate((rough - 0.1f) / 0.9f);
		float sample_dist = distance(primary_worldpos, hitpos.xyz);
		float grid_size_level = (1.0f / grid_size) * exp2(float(level));
		float score = saturate(sample_dist / grid_size_level * rough * rough);
		grid_size_level *= lerp(2.5f, 0.f, score);
		hitpos.xyz += onb.to_world(sampling::uniform_sample_hemisphere(sampler.next2f()) * float3(grid_size_level, grid_size_level, 0.f));
		if (temporal_get_grid_key(buffer_size, max_offset, hitpos.xyz, face, cam_pos, grid_size, key, level)) {
			key_slot = emplace_hash_slot(key_buffer, buffer_size, key, max_offset);
			if (key_slot != max_uint32) {
				mark_result = key_slot;
				auto slot = key_slot * SURFEL_INT_SIZE;
				uint confidence_int = saturate(confidence) * float(max_uint32);
				value_buffer.atomic_fetch_min(slot + 8u, confidence_int);
				auto old_frame_index = value_buffer.atomic_exchange(slot + 4u, frame_index);
				if (old_frame_index != frame_index) {
					value_buffer.write(slot + 5u, bit_cast<uint>(hitpos.x));
					value_buffer.write(slot + 6u, bit_cast<uint>(hitpos.y));
					value_buffer.write(slot + 7u, bit_cast<uint>(hitpos.z));
				}
				// auto old_count = value_buffer.atomic_fetch_add(slot + 3u, 1);
				// if (old_count < MAX_ACCUM) {

				// 	auto radiance_r = radiance_img.read(id);
				// 	old_count = old_count % 3;
				// 	for (uint i = old_count; i < old_count + 3; ++i) {
				// 		auto local_id = i;
				// 		if (local_id >= 3) {
				// 			local_id -= 3;
				// 		}
				// 		float_atomic_add(value_buffer, slot + local_id, radiance_r[local_id]);
				// 	}
				// }
			}
		}
	}
	surfel_mark.write(id, uint4(mark_result));
}