// #define DEBUG
// #include <luisa/printer.hpp>

#include <surfel/surfel_grid.hpp>
#include <utils/onb.hpp>
#include <sampling/pcg.hpp>
#include <sampling/sample_funcs.hpp>
#include <path_tracer/gbuffer.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<GBuffer>& gbuffers,
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	Image<uint>& surfel_mark,
	float2 jitter,
	float3 cam_pos,
	float grid_size,
	uint buffer_size,
	uint max_offset,
	uint max_accum) {
	auto id = dispatch_id().xy;
	auto size = dispatch_size().xy;
	uint buffer_id = (id.x + id.y * size.x);
	GBuffer gbuffer = gbuffers.read(buffer_id);
	// hit sky or roughness < 0.5
	if (all(float3(gbuffer.beta[0], gbuffer.beta[1], gbuffer.beta[2]) < -1e-5f)) {
		surfel_mark.write(id, uint4(max_uint32));
		return 0;
	}
	uint mark_result = max_uint32;
	uint encoded_normal = bit_cast<uint>(gbuffer.hitpos_normal[3]);
	float3 hitpos(gbuffer.hitpos_normal[0], gbuffer.hitpos_normal[1], gbuffer.hitpos_normal[2]);
	float2 oct_normal = float2((encoded_normal & 0xffff) / float(0xffff), (encoded_normal >> 16) / float(0xffff));
	auto hit_normal = sampling::decode_unit_vector(oct_normal);
	float3 abs_normal = abs(hit_normal);
	uint face = 0;
	if (abs_normal.y > abs_normal.z && abs_normal.y > abs_normal.x) {
		face = ite(hit_normal.y < 0, 4, 1);
	} else if (abs_normal.z > abs_normal.y && abs_normal.z > abs_normal.x) {
		face = ite(hit_normal.z < 0, 5, 2);
	} else {
		face = ite(hit_normal.x < 0, 3, 0);
	}

	uint key;
	uint key_slot;
	hitpos.xyz += hit_normal / (grid_size * 2.0f);
	int level;
	if (get_grid_level(cam_pos, grid_size, hitpos.xyz, level) &&
		temporal_get_grid_key(buffer_size, max_offset, hitpos.xyz, face, cam_pos, grid_size, key, level)) {
		key_slot = emplace_hash_slot(key_buffer, buffer_size, key, max_offset);
		if (key_slot != max_uint32) {
			mark_result = key_slot;
			auto slot = key_slot * OFFLINE_SURFEL_INT_SIZE;
			auto old_count = value_buffer.atomic_fetch_add(slot + 3u, 1);
			if (old_count < max_accum) {
				auto radiance = gbuffer.radiance;
				old_count = old_count % 3;
				for (uint i = old_count; i < old_count + 3; ++i) {
					auto local_id = i;
					if (local_id >= 3) {
						local_id -= 3;
					}
					float_atomic_add(value_buffer, slot + local_id, radiance[local_id]);
				}
			}
		}
	}
	surfel_mark.write(id, uint4(mark_result));
	return 0;
}
