#pragma once
#include <luisa/std.hpp>
#include <sampling/pcg.hpp>
using namespace luisa::shader;
#define HASH_EMPTY max_uint32

static uint hash(uint k, uint bufferSize) {
	sampling::PCGSampler sampler(k);
	return sampler.state & (bufferSize - 1);
}
static uint emplace_hash_slot(
	Buffer<uint>& keys,
	uint buffer_size,
	uint key,
	uint max_offset) {
	uint slot = hash(key, buffer_size);
	for (int i = 0; i < max_offset; ++i) {
		auto prev = keys.atomic_compare_exchange(slot, HASH_EMPTY, key);
		if (prev == HASH_EMPTY || prev == key) {
			return slot;
		}
		slot = (slot + 1) & (buffer_size - 1);
	}
	return max_uint32;
}

static uint emplace_hash_new_slot(
	Buffer<uint>& keys,
	uint buffer_size,
	uint key,
	uint max_offset,
	bool& is_new) {
	uint slot = hash(key, buffer_size);
	is_new = false;
	for (int i = 0; i < max_offset; ++i) {
		auto prev = keys.atomic_compare_exchange(slot, HASH_EMPTY, key);
		if (prev == HASH_EMPTY) {
			is_new = true;
			return slot;
		}
		if (prev == key) {
			is_new = false;
			return slot;
		}
		slot = (slot + 1) & (buffer_size - 1);
	}
	return max_uint32;
}

static uint hash_lookup(
	Buffer<uint>& keys,
	uint buffer_size,
	uint key,
	uint max_offset) {
	uint slot = hash(key, buffer_size);
	for (int i = 0; i < max_offset; ++i) {
		auto buffer_key = keys.read(slot);
		if (buffer_key == key) {
			return slot;
		}
		if (buffer_key == HASH_EMPTY) {
			return max_uint32;
		}
		slot = (slot + 1) & (buffer_size - 1);
	}
	return max_uint32;
}