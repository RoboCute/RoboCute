#include <rbc_core/binary_file_writer.h>
#include <rbc_world_v2/raw_data.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc::world {
RawData::RawData() = default;
RawData::~RawData() {}
bool RawData::init_device_resource() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    std::lock_guard lck{_async_mtx};
    if (!_device_buffer) return false;
    if (_device_buffer->_host_data.empty()) return false;
    _device_buffer->_buffer = render_device->lc_device().create_buffer<uint>(_device_buffer->_host_data.size() / sizeof(uint));
    return true;
}
bool RawData::loaded() const {
    std::shared_lock lck{_async_mtx};
    return _device_buffer && _device_buffer->loaded();
}
luisa::vector<std::byte> *RawData::host_data() const {
    std::shared_lock lck{_async_mtx};
    if (_device_buffer) return &_device_buffer->_host_data;
    return nullptr;
}
void RawData::create_empty(
    size_t size_bytes,
    bool upload_device) {
    std::lock_guard lck{_async_mtx};
    if (_device_buffer) {
        LUISA_ERROR("Raw data already created.");
    }
    _device_buffer->_host_data.push_back_uninitialized(size_bytes);
    _upload_device = upload_device;
    auto render_device = RenderDevice::instance_ptr();
    if (upload_device && render_device) {
        _device_buffer = new DeviceBuffer{};
        _device_buffer->_buffer = render_device->lc_device().create_buffer<uint>(size_bytes / sizeof(uint));
    }
}
void RawData::serialize(ObjSerialize const &obj) const {
    std::shared_lock lck{_async_mtx};
    ResourceBaseImpl<RawData>::serialize(obj);
    auto host_data_ = host_data();
    uint64_t size = host_data_ ? host_data_->size_bytes() : 0;

    obj.ser._store(size, "size_bytes");
    obj.ser._store(_upload_device, "upload_device");
}
void RawData::deserialize(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
    ResourceBaseImpl<RawData>::deserialize(obj);
    uint64_t size;
    obj.ser._load(size, "size_bytes");
    obj.ser._load(_upload_device, "upload_device");
    if (!_device_buffer)
        _device_buffer = new DeviceBuffer{};
    _device_buffer->_host_data.clear();
    _device_buffer->_host_data.push_back_uninitialized(size);
}
bool RawData::async_load_from_file() {
    wait_load();
    if (loaded()) {
        return false;
    }
    if (_path.empty()) return false;
    std::lock_guard lck{_async_mtx};
    if (_device_buffer)
        return false;
    _device_buffer = new DeviceBuffer{};
    _device_buffer->async_load_from_file(
        _path,
        _file_offset,
        _device_buffer->_host_data.size(),
        DeviceBuffer::FileLoadType::All);
    return true;
}
void RawData::unload() {
    std::lock_guard lck{_async_mtx};
    _device_buffer.reset();
}
void RawData::wait_load() const {
    std::shared_lock lck{_async_mtx};
    if (_device_buffer) {
        _device_buffer->wait_finished();
    }
}
bool RawData::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_device_buffer || _device_buffer->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(_path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_buffer->host_data());
    return true;
}

DECLARE_WORLD_OBJECT_REGISTER(RawData)
}// namespace rbc::world