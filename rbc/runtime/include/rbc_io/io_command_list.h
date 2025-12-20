#pragma once
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/volume.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include "dstorage_interface.h"

namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct IOTextureSubView {
	uint64_t handle;
	void* native_handle;
	std::array<uint, 3> offset;
	PixelStorage storage;
	std::array<uint, 3> size;
	uint level;
	template<typename T>
		requires(is_image_or_view_v<T>)
	IOTextureSubView(T const& img, uint2 offset, uint2 size) {
		handle = img.handle();
		native_handle = img.native_handle();
		storage = img.storage();
		this->offset = {offset.x, offset.y, 0u};
		this->size = {size.x, size.y, 1u};
		if constexpr (is_image_v<T>) {
			level = 0;
		} else {
			level = img.level();
		}
	}
	template<typename T>
		requires(is_image_or_view_v<T>)
	IOTextureSubView(T const& img)
	: IOTextureSubView(img, uint2(0), img.size())
	{

	}
	template<typename T>
		requires(is_volume_or_view_v<T>)
	IOTextureSubView(T const& volume, uint3 offset, uint3 size) {
		handle = volume.handle();
		native_handle = volume.native_handle();
		storage = volume.storage();
		this->offset = {offset.x, offset.y, offset.z};
		this->size = {size.x, size.y, size.z};
		if constexpr (is_volume_v<T>) {
			level = 0;
		} else {
			level = volume.level();
		}
	}
	template<typename T>
		requires(is_volume_or_view_v<T>)
	IOTextureSubView(T const& volume) 
	: IOTextureSubView(volume, uint3(0), volume.size())
	{
	}
	IOTextureSubView(
		uint64_t handle,
		void* native_handle,
		std::array<uint, 3> const& offset,
		PixelStorage storage,
		std::array<uint, 3> const& size,
		uint level)
		: handle(handle),
		  native_handle(native_handle),
		  offset(offset),
		  storage(storage),
		  size(size),
		  level(level) {}
	size_t size_bytes() const {
		return pixel_storage_size(storage, make_uint3(size[0], size[1], size[2]));
	}
};
struct IOBufferSubView {
	uint64_t handle;
	void* native_handle;
	uint64_t offset_bytes;
	uint64_t size_bytes;
	IOBufferSubView() {}
	template<typename T>
		requires(is_buffer_or_view_v<T>)
	IOBufferSubView(T const& buffer) {
		if constexpr (is_buffer_view_v<T>) {
			offset_bytes = buffer.offset_bytes();
		} else {
			offset_bytes = 0;
		}
		size_bytes = buffer.size_bytes();
		handle = buffer.handle();
		native_handle = buffer.native_handle();
	}
};
struct IOCommand {
	friend struct IOService;
	using SrcType = luisa::variant<
		IOFile::Handle,
		void const*>;
	SrcType file_data;
	size_t offset_bytes;
	using DestType = luisa::variant<
		IOBufferSubView,
		IOTextureSubView,
		vstd::span<std::byte>>;

	DestType dst;
	IOCommand(
		IOFile const& file,
		size_t offset_bytes,
		IOBufferSubView const& buffer_view)
		: file_data(file.handle()),
		  offset_bytes(offset_bytes),
		  dst(std::move(buffer_view)) {
	}
	IOCommand(
		IOFile const& file,
		size_t offset_bytes,
		IOTextureSubView const& tex_view)
		: file_data(file.handle()),
		  offset_bytes(offset_bytes),
		  dst(std::move(tex_view)) {
	}
	IOCommand(
		IOFile const& file,
		size_t offset_bytes,
		vstd::span<std::byte> dst)
		: file_data(file.handle()),
		  offset_bytes(offset_bytes),
		  dst(std::move(dst)) {
	}
	IOCommand(
		SrcType const& file,
		size_t offset_bytes,
		IOBufferSubView const& buffer_view)
		: file_data(file),
		  offset_bytes(offset_bytes),
		  dst(std::move(buffer_view)) {
	}
	IOCommand(
		SrcType const& file,
		size_t offset_bytes,
		IOTextureSubView const& tex_view)
		: file_data(file),
		  offset_bytes(offset_bytes),
		  dst(std::move(tex_view)) {
	}
	IOCommand(
		SrcType const& file,
		size_t offset_bytes,
		vstd::span<std::byte> dst)
		: file_data(file),
		  offset_bytes(offset_bytes),
		  dst(std::move(dst)) {
	}
	size_t size_bytes() const {
		return luisa::visit([&]<typename T>(T const& t) {
			if constexpr (std::is_same_v<T, IOBufferSubView>) {
				return t.size_bytes;
			} else {
				return t.size_bytes();
			}
		},
							dst);
	}
};

struct IOCommandList {
private:
	vector<IOCommand> _commands;
	vector<IOFile> _dispose_files;
	vector<vstd::function<void()>> _callbacks;

public:
	[[nodiscard]] bool empty() const { return _commands.empty() && _dispose_files.empty() && _callbacks.empty(); }
	[[nodiscard]] span<IOCommand const> commands() const { return _commands; }
	[[nodiscard]] auto&& steal_commands() && { return std::move(_commands); }
	[[nodiscard]] auto&& steal_files() && { return std::move(_dispose_files); }
	[[nodiscard]] auto&& steal_callbacks() && { return std::move(_callbacks); }
	IOCommandList& dispose_file(IOFile&& file) {
		_dispose_files.emplace_back(std::move(file));
		return *this;
	}
	IOCommandList& operator<<(IOCommand&& cmd) {
		_commands.emplace_back(std::move(cmd));
		return *this;
	}
	IOCommandList& add_callback(vstd::function<void()>&& func) {
		_callbacks.emplace_back(std::move(func));
		return *this;
	}
	template<typename... Args>
		requires(luisa::is_constructible_v<IOFile, Args && ...>)
	IOFile::Handle retrieve(Args&&... args) {
		auto&& f = _dispose_files.emplace_back(std::forward<Args>(args)...);
		return f.handle();
	}
};
}// namespace rbc