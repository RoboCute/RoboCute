#pragma once
#include <rbc_world/resources/texture.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world/type_register.h>
namespace rbc::world {

TextureResource::TextureResource() = default;
TextureResource::~TextureResource() = default;
luisa::vector<std::byte> *TextureResource::host_data() {
    std::shared_lock lck{_async_mtx};
    if (_tex && _tex->resource_type() == DeviceResource::Type::Image) {
        return &static_cast<DeviceImage *>(_tex.get())->host_data_ref();
    }
    return nullptr;
}
DeviceImage *TextureResource::get_image() const {
    std::shared_lock lck{_async_mtx};
    if (_tex && !is_vt()) {
        return static_cast<DeviceImage *>(_tex.get());
    }
    LUISA_ERROR("Getting non-sparse image from virtual texture");
    return nullptr;
}
DeviceSparseImage *TextureResource::get_sparse_image() const {
    std::shared_lock lck{_async_mtx};
    if (_tex && is_vt()) {
        return static_cast<DeviceSparseImage *>(_tex.get());
    }
    LUISA_ERROR("Getting sparse image from non-virtual texture");
    return nullptr;
}
uint64_t TextureResource::desire_size_bytes() const {
    auto size = _size;
    uint64_t size_bytes{};
    for (auto i : vstd::range(_mip_level)) {
        size_bytes += pixel_storage_size(
            (PixelStorage)_pixel_storage,
            make_uint3(size, 1u));
        size = max(uint2(1), size >> 1u);
    }
    return size_bytes;
}
void TextureResource::serialize_meta(ObjSerialize const &obj) const {
    std::shared_lock lck{_async_mtx};
    BaseType::serialize_meta(obj);
    obj.ser._store(_pixel_storage, "pixel_storage");
    obj.ser._store(_size, "size");
    obj.ser._store(_mip_level, "mip_level");
    obj.ser._store(_is_vt, "is_vt");
}
void TextureResource::deserialize_meta(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
    BaseType::deserialize_meta(obj);
#define RBC_MESH_LOAD(m)            \
    {                               \
        decltype(_##m) m;           \
        if (obj.ser._load(m, #m)) { \
            _##m = m;               \
        }                           \
    }
    RBC_MESH_LOAD(pixel_storage)
    RBC_MESH_LOAD(mip_level)
    RBC_MESH_LOAD(size)
    RBC_MESH_LOAD(is_vt)
#undef RBC_MESH_LOAD
}
void TextureResource::create_empty(
    luisa::filesystem::path &&path,
    uint64_t file_offset,
    LCPixelStorage pixel_storage,
    luisa::uint2 size,
    uint32_t mip_level,
    bool is_vt) {
    _status = EResourceLoadingStatus::Loading;
    if (_tex) [[unlikely]] {
        LUISA_ERROR("Can not create on exists texture.");
    }
    std::lock_guard lck{_async_mtx};
    _path = std::move(path);
    _file_offset = file_offset;
    _size = size;
    _pixel_storage = pixel_storage;
    _mip_level = mip_level;
    _is_vt = is_vt;
    if (is_vt) {
        _tex = new DeviceSparseImage();
        _vt_finished = new VTLoadFlag{};
    } else {
        _tex = new DeviceImage();
    }
}
bool TextureResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_tex || _tex->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(_path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_tex->host_data());
    return true;
}
bool TextureResource::is_vt() const {
    if (_tex) {
        return _tex->resource_type() == DeviceResource::Type::SparseImage;
    } else {
        return _is_vt;
    }
}
bool TextureResource::init_device_resource() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device || !_tex || load_executed()) {
        return false;
    }
    if (_is_vt != is_vt()) {
        return false;
    }
    auto file_size = desire_size_bytes();
    auto host_data_ = host_data();
    LUISA_ASSERT(!host_data_ || host_data_->empty() || host_data_->size() == file_size, "Invalid host data length {}, requires {}.", host_data_->size(), file_size);
    std::lock_guard lck{_async_mtx};
    if (is_vt()) {
        if (_path.empty()) {
            LUISA_WARNING("Virtual texture must have path.");
            return false;
        }
        auto tex = static_cast<DeviceSparseImage *>(_tex.get());
        tex->load(
            TexStreamManager::instance(),
            [vt_finished = this->_vt_finished]() {
                vt_finished->finished = true;
            },
            _path,
            _file_offset,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level);
    } else {
        auto tex = static_cast<DeviceImage *>(_tex.get());
        tex->create_texture<float>(
            render_device->lc_device(),
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level);
    }
    _status = EResourceLoadingStatus::Loaded;
    return true;
}
bool TextureResource::_async_load_from_file() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    auto file_size = desire_size_bytes();
    if (_path.empty()) {
        return false;
    }
    std::lock_guard lck{_async_mtx};
    if (_tex) {
        return false;
    }
    if (is_vt()) {
        auto tex = new DeviceSparseImage();
        _tex = tex;
        _vt_finished = new VTLoadFlag{};
        tex->load(
            TexStreamManager::instance(),
            [vt_finished = this->_vt_finished]() {
                vt_finished->finished = true;
            },
            _path,
            _file_offset,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level);
    } else {
        auto tex = new DeviceImage();
        _tex = tex;
        tex->async_load_from_file(
            _path,
            _file_offset,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level,
            DeviceImage::ImageType::Float,
            !tex->host_data_ref().empty());
    }
    return true;
}

void TextureResource::_unload() {
    std::lock_guard lck{_async_mtx};
    _tex.reset();
}
uint32_t TextureResource::heap_index() const {
    std::shared_lock lck{_async_mtx};
    if (!_tex) return ~0u;
    if (is_vt()) {
        return static_cast<DeviceSparseImage *>(_tex.get())->heap_idx();
    } else {
        return static_cast<DeviceImage *>(_tex.get())->heap_idx();
    }
}
rbc::coroutine TextureResource::_async_load() {
    if (!_async_load_from_file()) co_return;
    while (!_load_finished()) {
        co_await std::suspend_always{};
    }
    _status = EResourceLoadingStatus::Loaded;
    co_return;
}
bool TextureResource::load_executed() const {
    std::shared_lock lck{_async_mtx};
    if (!_tex) return false;
    if (is_vt()) {
        return static_cast<DeviceSparseImage *>(_tex.get())->load_executed();
    } else {
        return static_cast<DeviceImage *>(_tex.get())->load_executed() || static_cast<DeviceImage *>(_tex.get())->type() != DeviceImage::ImageType::None;
    }
}
bool TextureResource::_load_finished() const {
    std::shared_lock lck{_async_mtx};
    if (!_tex)
        return false;
    if (is_vt()) {
        auto vt = static_cast<DeviceSparseImage *>(_tex.get());
        auto result = vt->load_executed() && _vt_finished->finished;
        return result;
    } else {
        return static_cast<DeviceImage *>(_tex.get())->load_finished();
    }
}
DECLARE_WORLD_OBJECT_REGISTER(TextureResource)

}// namespace rbc::world