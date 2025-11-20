#pragma once
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_config.h>
#include <luisa/core/binary_io.h>
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API DeviceBuffer : DeviceResource {
    Buffer<uint> _buffer;
    Type resource_type() const override { return Type::Buffer; }
    template <typename T>
    [[nodiscard]] BufferView<T> get_buffer() const
    {
        return _buffer.view().as<T>();
    }
    DeviceBuffer();
    ~DeviceBuffer();
    void async_load_from_file(
        luisa::filesystem::path const& path,
        size_t file_offset
    );
    void async_load_from_memory(luisa::vector<BinaryBlob>&& blobs);
    void async_load_from_memory(BinaryBlob&& blob);
};
} // namespace rbc