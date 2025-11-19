#include <rbc_graphics/buffer_allocator.h>
namespace rbc {
BufferAllocator::BufferAllocator(Device& device, uint capacity)
	: _fit(static_cast<size_t>(std::numeric_limits<uint>::max()) * sizeof(uint), alignof(uint)),
	  _buffer(device.create_buffer<uint>(capacity / sizeof(uint))),
	  _capacity(capacity) {
}
auto BufferAllocator::allocate(
	Device& device,
	CommandList& cmd_list,
	DisposeQueue& disp_queue,
	uint size,
	vstd::FuncRef<void(Buffer<uint> const& old_buffer, Buffer<uint> const& new_buffer)> before_copy,
	AllocateType alloc_type) -> Node {
	size = (size + 3) & (~3);
	auto fit_node = [&]() {
		if (alloc_type == AllocateType::FirstFit) {
			return _fit.allocate(size);
		} else {
			return _fit.allocate(size);
		}
	}();
	size_t buffer_size = fit_node->offset() + fit_node->size();
	if (buffer_size > _capacity) {
		size_t new_capa = _capacity;
		do {
			new_capa *= 2;
		} while (buffer_size > new_capa);
		_capacity = new_capa;
		auto buffer_size = _capacity / sizeof(uint);
		if (_buffer.size() < buffer_size) {
			_dirty = true;
			auto new_buffer = device.create_buffer<uint>(buffer_size);
			before_copy(_buffer, new_buffer);
			cmd_list << new_buffer.view(0, _buffer.size()).copy_from(_buffer);
			disp_queue.dispose_after_queue(std::move(_buffer));
			_buffer = std::move(new_buffer);
		}
	}
	Node n;
	n._fit_node = fit_node;
	return n;
}
void BufferAllocator::free(Node node) {
	_fit.free(node._fit_node);
}
BufferAllocator::~BufferAllocator() {
}
}// namespace rbc