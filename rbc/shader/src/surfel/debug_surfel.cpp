#define DEBUG
#include <surfel/surfel_grid.hpp>
#include <utils/onb.hpp>
#include <sampling/pcg.hpp>
#include <sampling/sample_funcs.hpp>
#include <luisa/printer.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& depth_img,
	Image<float>& normal_rough,
	Image<float>& radiance_img,
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	float4x4 invProj,
	float4x4 invView,
	float2 jitter_offset,
	float3 cam_pos,
	float grid_size,
	uint buffer_size,
	uint max_offset) {
	auto coord = dispatch_id().xy;
	auto size = dispatch_size().xy;
	auto depth = depth_img.read(coord).x;
	float3 world_pos = sampling::view_to_world(invProj, invView, (float2(coord) + jitter_offset + float2(0.5)) / float2(size), depth);
	float3 radiance;
	if (depth < 1e5f) {
		auto normal = normal_rough.read(coord).xy;
		auto encode_normal = (uint(normal.x * float(0xffff)) & 0xffff) + (uint(normal.y * float(0xffff)) << 16);
		auto geo_normal = sampling::decode_unit_vector(normal);

		auto uv = (float2(coord) + float2(0.5)) / float2(size);
		auto proj = float4((uv * 2.f - 1.0f), depth, 1);
		uint key;
		world_pos.xyz += geo_normal / (grid_size * 2.0f);
		// geo_normal = float3(1, 0, 0);
		if (get_grid_key(buffer_size, max_offset, world_pos.xyz, geo_normal, cam_pos, grid_size, key)) {
			uint slot = hash_lookup(key_buffer, buffer_size, key, max_offset);
			if (slot != max_uint32) {
				sampling::PCGSampler pcg(slot);
				// radiance = pcg.next3f();
				radiance = float3(
					bit_cast<float>(value_buffer.read(slot * SURFEL_INT_SIZE + 0)),
					bit_cast<float>(value_buffer.read(slot * SURFEL_INT_SIZE + 1)),
					bit_cast<float>(value_buffer.read(slot * SURFEL_INT_SIZE + 2)));
				radiance /= min(float(MAX_ACCUM), float(value_buffer.read(slot * SURFEL_INT_SIZE + 3)));
			}
		}
	}
	radiance_img.write(coord, float4(radiance, 1.0f));
	return 0;
}