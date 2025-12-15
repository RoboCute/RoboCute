#pragma once
#include <rbc_world_v2/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world_v2/type_register.h>
namespace rbc::world {
Texture::Texture() = default;
Texture::~Texture() = default;
luisa::vector<std::byte> *Texture::host_data() {
    if (_tex && _tex->resource_type() == DeviceResource::Type::Image) {
        return &static_cast<DeviceImage *>(_tex.get())->host_data_ref();
    }
    return nullptr;
}
DeviceImage *Texture::get_image() const {
    if (_tex && !_is_vt) {
        return static_cast<DeviceImage *>(_tex.get());
    }
    return nullptr;
}
DeviceSparseImage *Texture::get_sparse_image() const {
    if (_tex && _is_vt) {
        return static_cast<DeviceSparseImage *>(_tex.get());
    }
    return nullptr;
}
uint64_t Texture::desire_size_bytes() const {
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
void Texture::serialize(ObjSerialize const &obj) const {
    BaseType::serialize(obj);
    obj.ser._store(_pixel_storage, "pixel_storage");
    obj.ser._store(_size, "size");
    obj.ser._store(_mip_level, "mip_level");
    obj.ser._store(_is_vt, "is_vt");
}
void Texture::deserialize(ObjDeSerialize const &obj) {
    BaseType::deserialize(obj);
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
void Texture::create_empty(
    luisa::filesystem::path &&path,
    uint64_t file_offset,
    LCPixelStorage pixel_storage,
    luisa::uint2 size,
    uint32_t mip_level,
    bool is_vt) {
    std::lock_guard lck{_async_mtx};
    wait_load();
    if (loaded()) [[unlikely]] {
        LUISA_ERROR("Can not create on exists mesh.");
    }
    _path = std::move(path);
    _file_offset = file_offset;
    _size = size;
    _pixel_storage = pixel_storage;
    _mip_level = mip_level;
    _is_vt = is_vt;
}
bool Texture::init_device_resource() {
    std::lock_guard lck{_async_mtx};
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    wait_load();
    if (loaded()) {
        return false;
    }
    auto file_size = desire_size_bytes();
    LUISA_ASSERT(host_data()->empty() || host_data()->size() == file_size, "Invalid host data length {}, requires {}.", host_data()->size(), file_size);
    if (_is_vt) {
        if (_path.empty()) {
            LUISA_WARNING("Virtual texture must have path.");
            return false;
        }
        DeviceSparseImage *tex{};
        if (_is_vt && _tex) {
            tex = static_cast<DeviceSparseImage *>(_tex.get());
        } else {
            tex = new DeviceSparseImage();
            _tex = tex;
        }
        if (!_vt_finished) {
            _vt_finished = new VTLoadFlag{};
        }
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
        DeviceImage *tex{};
        if (!_is_vt && _tex) {
            tex = static_cast<DeviceImage *>(_tex.get());
        } else {
            tex = new DeviceImage();
            _tex = tex;
        }
        tex->create_texture<float>(
            render_device->lc_device(),
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level);
    }
    return true;
}
bool Texture::async_load_from_file() {
    std::lock_guard lck{_async_mtx};
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    auto file_size = desire_size_bytes();
    if (_path.empty()) {
        return false;
    }
    LUISA_ASSERT(!host_data() || host_data()->empty() || host_data()->size() == file_size, "Invalid host data length.");
    if (_tex) {
        return false;
    }
    if (_is_vt) {
        auto tex = static_cast<DeviceSparseImage *>(_tex.get());
        if (!_vt_finished) {
            _vt_finished = new VTLoadFlag{};
        }
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
void Texture::wait_load() const {
    if (!_tex) return;
    _tex->wait_finished();
    if (_vt_finished) {
        while (!_vt_finished->finished) {
            std::this_thread::yield();
        }
    }
}
void Texture::unload() {
    _tex.reset();
}
uint32_t Texture::heap_index() const {
    if (!_tex) return ~0u;
    if (_is_vt) {
        return static_cast<DeviceSparseImage *>(_tex.get())->heap_idx();
    } else {
        return static_cast<DeviceImage *>(_tex.get())->heap_idx();
    }
}
bool Texture::loaded() const {
    if (!_tex) return false;
    if (_is_vt) {
        return static_cast<DeviceSparseImage *>(_tex.get())->loaded() && _vt_finished->finished;
    } else {
        return static_cast<DeviceImage *>(_tex.get())->loaded();
    }
}
DECLARE_WORLD_OBJECT_REGISTER(Texture)

}// namespace rbc::world