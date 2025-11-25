#pragma once
#include <rbc_config.h>
#include "first_fit.h"
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/device.h>
#include "dispose_queue.h"
#include <luisa/runtime/command_list.h>
#include <luisa/core/shared_function.h>
#include <luisa/vstl/functional.h>

namespace rbc {
using namespace luisa::compute;
struct RBC_RUNTIME_API BufferAllocator {
private:
	rbc::FirstFit _fit;
	// thread-safe area
	Buffer<uint> _buffer;
	size_t _capacity;
	bool _dirty{false};

public:
	enum struct AllocateType : uint32_t {
		FirstFit,
		BestFit
	};
	struct Node {
		friend struct BufferAllocator;

	private:
		rbc::FirstFit::Node* _fit_node;

	public:
		Node() : _fit_node(nullptr) {}
		Node(Node const&) = default;
		template<typename T>
		BufferView<T> view(BufferAllocator& a) const {
			auto uint_view = a._buffer.view(_fit_node->offset() / sizeof(uint), _fit_node->size() / sizeof(uint));
			if constexpr (std::is_same_v<T, uint>) {
				return uint_view;
			} else {
				return uint_view.as<T>();
			}
		}
		[[nodiscard]] auto offset_bytes() const noexcept { return _fit_node->offset(); }
		[[nodiscard]] auto size_bytes() const noexcept { return _fit_node->size(); }
		operator bool() const {
			return _fit_node != nullptr;
		}
	};
	BufferAllocator(Device& device, uint capacity = 65536);
	Node allocate(
		Device& device,
		CommandList& cmd_list,
		DisposeQueue& disp_queue,
		uint size_bytes,
		vstd::FuncRef<void(Buffer<uint> const& old_buffer, Buffer<uint> const& new_buffer)> before_copy,
		AllocateType alloc_type = AllocateType::FirstFit);
	Buffer<uint> const& buffer() const { return _buffer; }
	void free(Node node);
	~BufferAllocator();
	void mark_clean() {_dirty = false;}
	bool dirty() const {return _dirty;}
};
}// namespace rbc