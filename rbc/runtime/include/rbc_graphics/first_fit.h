#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/string.h>
#include <luisa/vstl/pool.h>
namespace rbc {
struct RBC_RUNTIME_API FirstFit {

public:
	struct RBC_RUNTIME_API Node {

	private:
		Node* _next{nullptr};
		size_t _offset{0u};
		size_t _size{0u};

	private:
		friend struct FirstFit;

	public:
		Node() noexcept;
		Node(Node&&) noexcept = delete;
		Node(const Node&) noexcept = delete;
		Node& operator=(Node&&) noexcept = delete;
		Node& operator=(const Node&) noexcept = delete;
		[[nodiscard]] auto offset() const noexcept { return _offset; }
		[[nodiscard]] auto size() const noexcept { return _size; }
	};

private:
	Node _free_list;
	size_t _alignment;
	vstd::Pool<Node, true> _node_pool;

private:
	void _destroy() noexcept;

public:
	explicit FirstFit(size_t size, size_t alignment) noexcept;
	~FirstFit() noexcept;
	FirstFit(FirstFit&&) noexcept;
	FirstFit(const FirstFit&) noexcept = delete;
	[[nodiscard]] Node* allocate(size_t size) noexcept;
	[[nodiscard]] Node* allocate_best_fit(size_t size) noexcept;
	void free(Node* node) noexcept;
	[[nodiscard]] auto size() const noexcept { return _free_list._size; }
	[[nodiscard]] auto alignment() const noexcept { return _alignment; }
	void clean_all() noexcept;
};
}// namespace rbc