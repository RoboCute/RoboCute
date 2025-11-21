#pragma once
#include <luisa/std.hpp>
#include <luisa/resources/bindless_array.hpp>
#include <luisa/printer.hpp>
#include <geometry/types.hpp>
#include <sampling/pcg.hpp>

namespace material {
using namespace luisa::shader;
struct MatMeta {
	uint mat_type;
	uint mat_index;
};
}// namespace material

namespace material {
inline uint _mat_code(
	BindlessBuffer& heap,
	uint mat_idx_buffer_heap_idx,
	uint submesh_heap_idx,
	uint mat_index,
	uint prim_id) {
	uint submesh_idx;
	if (submesh_heap_idx != max_uint32) {
		submesh_idx = heap.buffer_read<uint>(submesh_heap_idx, prim_id);
		mat_index = heap.uniform_idx_buffer_read<uint>(mat_idx_buffer_heap_idx, mat_index + submesh_idx);
	}
	return mat_index;
}
inline MatMeta mat_meta(
	uint mat_code) {
	MatMeta meta;
	meta.mat_type = (mat_code >> uint(24));
	meta.mat_index = mat_code & ((uint(1) << uint(24)) - uint(1));
	return meta;
}
inline MatMeta mat_meta(
	BindlessBuffer& heap,
	uint mat_idx_buffer_heap_idx,
	uint submesh_heap_idx,
	uint mat_index,
	uint prim_id) {
	return mat_meta(_mat_code(heap, mat_idx_buffer_heap_idx, submesh_heap_idx, mat_index, prim_id));
}
class HandleBase {
	uint32 handle;

public:
	constexpr HandleBase(uint32 handle = max_uint32) noexcept : handle(handle) {}

	static constexpr HandleBase invalid() noexcept {
		return HandleBase{max_uint32};
	}

	constexpr bool valid() const noexcept {
		return handle != max_uint32;
	}

	constexpr operator uint32() const noexcept {
		return handle;
	}
};

using MatImageHandle = HandleBase;
using MatVolumeHandle = HandleBase;
using MatBufferHandle = HandleBase;
}// namespace material