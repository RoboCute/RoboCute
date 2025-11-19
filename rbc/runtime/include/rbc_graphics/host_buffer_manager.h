#pragma once
#include <rbc_config.h>
#include <luisa/backends/ext/pinned_memory_ext.hpp>
#include <luisa/vstl/stack_allocator.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/buffer.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
template<typename T>
struct UploadBuffer {
	friend struct HostBufferManager;
	BufferView<T> view;
	[[nodiscard]] auto mapped_ptr() const {
		return reinterpret_cast<T const*>(reinterpret_cast<std::byte const*>(view.native_handle()) + view.offset_bytes());
	}
	[[nodiscard]] auto mapped_ptr() {
		return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(view.native_handle()) + view.offset_bytes());
	}
	UploadBuffer(UploadBuffer const&) = default;

private:
	UploadBuffer(BufferView<T>&& view) : view(view) {}
};
struct RBC_RUNTIME_API HostBufferManager : public vstd::StackAllocatorVisitor {
	Device& _device;
	PinnedMemoryExt* ext;
	uint64 allocate(uint64 size) override;
	void deallocate(uint64 handle) override;
	vstd::vector<Buffer<uint>> _buffers;
	vstd::vector<uint64> _buffer_pool;
	vstd::vector<Buffer<uint>> _dispose_list;
	vstd::StackAllocator _alloc;
	BufferView<uint> _alloc_buffer(size_t byte_size, size_t align);

public:
	void clear();
	HostBufferManager(Device& device);
	HostBufferManager(HostBufferManager&&) = delete;
	HostBufferManager(HostBufferManager const&) = delete;

	~HostBufferManager();
	template<typename T>
		requires(alignof(T) >= 4 && alignof(T) <= 16)
	UploadBuffer<T> allocate_upload_buffer(size_t element) {
		return _alloc_buffer(element * sizeof(T), alignof(T)).as<T>();
	}
	template<typename T>
		requires(alignof(T) >= 4 && alignof(T) <= 16)
	UploadBuffer<T> allocate_upload_buffer(size_t element, size_t alignment) {
		return _alloc_buffer(element * sizeof(T), alignment).as<T>();
	}
	UploadBuffer<uint> allocate_upload_buffer(size_t size_bytes, size_t alignment) {
		return _alloc_buffer(size_bytes, alignment).as<uint>();
	}
	void flush();
};
}// namespace rbc