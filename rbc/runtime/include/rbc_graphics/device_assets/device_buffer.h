#pragma once
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_config.h>
#include <luisa/core/binary_io.h>
namespace rbc {
namespace world {
struct BufferResource;
}// namespace world
struct DisposeQueue;
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API DeviceBuffer : DeviceResource {
    friend struct world::BufferResource;
private:
    Buffer<uint> _buffer;
    luisa::vector<std::byte> _host_data;
public:
    [[nodiscard]] luisa::span<std::byte> host_data() override {
        return _host_data;
    }
    [[nodiscard]] luisa::span<std::byte const> host_data() const override {
        return _host_data;
    }
    Buffer<uint> const &buffer() const { return _buffer; }
    Type resource_type() const override { return Type::Buffer; }
    template<typename T>
    [[nodiscard]] BufferView<T> get_buffer() const {
        return _buffer.view().as<T>();
    }
    DeviceBuffer();
    ~DeviceBuffer();
    enum struct FileLoadType {
        None = 0,
        HostOnly = 1,
        DeviceOnly = 2,
        All = -1
    };
    void create_empty(uint64_t size_bytes, FileLoadType load_type);
    void async_load_from_file(
        luisa::filesystem::path const &path,
        size_t file_offset,
        uint64_t file_size,
        FileLoadType load_type);
    void async_load_from_memory(luisa::vector<BinaryBlob> &&blobs);
    void async_load_from_memory(BinaryBlob &&blob);
    // Host
    void discard_host();
    void sync_host_size_to_device();
    // Device
    void discard_device_unsafe(DisposeQueue *disp_queue);        // Unsafe! can crash GPU device
    void sync_buffer_size_to_device_unsafe(DisposeQueue* disp_queue);// Unsafe! can crash GPU device
};
}// namespace rbc