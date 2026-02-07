#include <rbc_world/resources/buffer.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_graphics/device_assets/device_buffer.h>
#include <rbc_graphics/render_device.h>
#include <rbc_world/type_register.h>
#include <rbc_core/utils/thread_waiter.h>

namespace rbc::world {

BufferResource::BufferResource() = default;

BufferResource::~BufferResource() = default;

bool BufferResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return !_device_buffer;
}

luisa::vector<std::byte> *BufferResource::host_data() {
    std::shared_lock lck{_async_mtx};
    if (_device_buffer) {
        return &_device_buffer->_host_data;
    }
    return nullptr;
}

DeviceBuffer *BufferResource::device_buffer() const {
    std::shared_lock lck{_async_mtx};
    return _device_buffer.get();
}

void BufferResource::create_empty(uint64_t size_bytes, bool create_device_buffer) {
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last buffer loading.");
    }
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    std::lock_guard lck{_async_mtx};
    _device_buffer.reset();
    _device_buffer = new DeviceBuffer{};
    _create_device_buffer = create_device_buffer;
    if (size_bytes > 0) {
        _device_buffer->_host_data.push_back_uninitialized(size_bytes);
    }
    _size_bytes = size_bytes;
    unsafe_set_loaded();
}

void BufferResource::serialize_meta(ObjSerialize const &obj) const {
    std::shared_lock lck{_async_mtx};
    obj.ar.value(_size_bytes, "size_bytes");
    obj.ar.value(_create_device_buffer, "create_device_buffer");
}

void BufferResource::deserialize_meta(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
    uint64_t size_bytes = 0;
    if (obj.ar.value(size_bytes, "size_bytes")) {
        _size_bytes = size_bytes;
    } else {
        _size_bytes = 0;
    }
    bool create_device_buffer = false;
    if (obj.ar.value(create_device_buffer, "create_device_buffer")) {
        _create_device_buffer = create_device_buffer;
    } else {
        _create_device_buffer = false;
    }
}

luisa::compute::BufferView<uint> BufferResource::buffer() const {
    return _device_buffer ? _device_buffer->_buffer : luisa::compute::BufferView<uint>{};
}

bool BufferResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_device_buffer || _device_buffer->_host_data.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_buffer->_host_data);
    return true;
}

rbc::coroutine BufferResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;
    auto path = this->path();
    if (path.empty()) {
        co_return;
    }
    std::lock_guard lck{_async_mtx};
    if (_device_buffer) {
        co_return;
    }
    LUISA_ASSERT((_size_bytes & (sizeof(uint) - 1)) == 0, "Buffer size must be aligned to 4 bytes");
    _device_buffer = new DeviceBuffer{};
    _device_buffer->async_load_from_file(
        path,
        0,
        _size_bytes,
        _create_device_buffer ? DeviceBuffer::FileLoadType::All : DeviceBuffer::FileLoadType::HostOnly);

    while (!_device_buffer->load_finished()) {
        co_await std::suspend_always{};
    }
    unsafe_set_installed();
    co_return;
}

DECLARE_WORLD_OBJECT_REGISTER(BufferResource)

}// namespace rbc::world
