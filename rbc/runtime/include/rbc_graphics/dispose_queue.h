#pragma once
#include <rbc_config.h>
#include <luisa/runtime/command_list.h>
#include <luisa/runtime/stream.h>
#include <luisa/vstl/common.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API DisposeQueue {
	struct Element {
		void* ptr;
		vstd::func_ptr_t<void(void*)> dtor;
	};
	vstd::vector<Element> _elements;

public:
	template<typename T>
		requires(!std::is_reference_v<T> && std::is_move_constructible_v<T> && !std::is_trivially_destructible_v<T>)
	void dispose_after_queue(T&& t) {
		auto& e = _elements.emplace_back();
		e.ptr = vengine_malloc(sizeof(T));
		e.dtor = [](void* ptr) {
			reinterpret_cast<T*>(ptr)->~T();
		};
		new (e.ptr) T(std::move(t));
	}
	template<typename T, typename Mtx>
		requires(!std::is_reference_v<T> && std::is_move_constructible_v<T> && !std::is_trivially_destructible_v<T>)
	void dispose_after_queue(T&& t, Mtx& mtx) {
		Element e;
		e.ptr = vengine_malloc(sizeof(T));
		e.dtor = [](void* ptr) {
			reinterpret_cast<T*>(ptr)->~T();
		};
		new (e.ptr) T(std::move(t));
		std::lock_guard<Mtx> lck{mtx};
		_elements.emplace_back(e);
	}
	void on_frame_end(CommandList& cmdlist);
	void on_frame_end(Stream& stream);
	void force_clear();
	DisposeQueue() = default;
	DisposeQueue(DisposeQueue const&) = delete;
	DisposeQueue(DisposeQueue&&) = default;
};
}// namespace rbc