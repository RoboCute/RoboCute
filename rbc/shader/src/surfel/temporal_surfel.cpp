// #define DEBUG
// #include <luisa/printer.hpp>
#include <surfel/hash_table.hpp>
using namespace luisa::shader;
#include <surfel/surfel_grid.hpp>

[[kernel_1d(128)]] int kernel(
	Buffer<uint>& curr_key_buffer,
	Buffer<uint>& curr_value_buffer,
	Buffer<uint>& last_key_buffer,
	Buffer<uint>& last_value_buffer,
	float3 curr_cam_pos,
	float3 last_cam_pos,
	uint map_size,
	uint frame_index,
	float grid_size,
	uint max_offset) {
	auto id = dispatch_id().x;
	auto last_key = last_key_buffer.read(id);
	if (last_key == HASH_EMPTY) return 0;
	auto value_id = id * SURFEL_INT_SIZE;
	auto last_confidence = float(last_value_buffer.read(value_id + 8u)) / float(max_uint32);
	auto last_frame_index = last_value_buffer.read(value_id + 4u);
	float blend_ratio = (1.0f - (1.0f / MAX_ACCUM)) * last_confidence;
	blend_ratio *= lerp(0.99f, 0.1f, saturate(float(frame_index - last_frame_index) - 1.0f));
	blend_ratio = pow(blend_ratio, float(frame_index) - float(last_frame_index));
	auto frame_diff = frame_index - last_frame_index;
	if (frame_diff >= 16) {
		return 0;
	}
	auto last_level = (last_key & 255u);
	last_key >>= 8u;
	auto last_face = (last_key & 7u);
	float3 last_pos;
	last_pos.x = bit_cast<float>(last_value_buffer.read(value_id + 5u));
	last_pos.y = bit_cast<float>(last_value_buffer.read(value_id + 6u));
	last_pos.z = bit_cast<float>(last_value_buffer.read(value_id + 7u));
	uint key;
	int curr_level;
	if (!temporal_get_grid_key(
			map_size,
			max_offset,
			last_pos,
			last_face,
			curr_cam_pos,
			grid_size,
			key,
			curr_level)) {
		return 0;
	}
	if (curr_level < last_level) {
		return 0;
	}
	float3 last_color(
		bit_cast<float>(last_value_buffer.read(value_id + 0)),
		bit_cast<float>(last_value_buffer.read(value_id + 1)),
		bit_cast<float>(last_value_buffer.read(value_id + 2)));
	uint last_accum = last_value_buffer.read(value_id + 3);
	uint curr_accum;
	curr_accum = uint(max(1.01f, min(float(last_accum), MAX_ACCUM * 0.5f) * blend_ratio));
	last_color *= float(curr_accum) / float(last_accum);
	bool is_new;
	if (frame_diff >= 3) {
		max_offset = 1u;
	} else if (frame_diff >= 2) {
		max_offset = max(1u, max_offset / 2u);
	}
	uint slot = emplace_hash_new_slot(curr_key_buffer, map_size, key, max_offset, is_new);
	if (slot == max_uint32 || (!is_new)) return 0;
	slot *= SURFEL_INT_SIZE;
	curr_value_buffer.write(slot + 0, bit_cast<uint>(last_color.x));
	curr_value_buffer.write(slot + 1, bit_cast<uint>(last_color.y));
	curr_value_buffer.write(slot + 2, bit_cast<uint>(last_color.z));
	curr_value_buffer.write(slot + 3, curr_accum);
	curr_value_buffer.write(slot + 4, last_frame_index);
	return 0;
}