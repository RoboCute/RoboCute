#pragma once
#include <rbc_world_v2/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/render_device.h>
#include "type_register.h"
namespace rbc::world {
struct TextureImpl : Texture {
    luisa::spin_mutex _async_mtx;
    TextureImpl() {
    }
    [[nodiscard]] luisa::vector<std::byte> *host_data() override {
        if (_device_image)
            return &_device_image->host_data_ref();
        return nullptr;
    }
    uint64_t desire_size_bytes() override {
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
    void rbc_objser(JsonSerializer &ser) const override {
        BaseType::rbc_objser(ser);
        ser._store(_pixel_storage, "pixel_storage");
        ser._store(_size, "size");
        ser._store(_mip_level, "mip_level");
    }
    void rbc_objdeser(JsonDeSerializer &ser) override {
        BaseType::rbc_objdeser(ser);
#define RBC_MESH_LOAD(m)        \
    {                           \
        decltype(_##m) m;       \
        if (ser._load(m, #m)) { \
            _##m = m;           \
        }                       \
    }
        RBC_MESH_LOAD(pixel_storage)
        RBC_MESH_LOAD(mip_level)
        RBC_MESH_LOAD(size)
#undef RBC_MESH_LOAD
    }
    void create_empty(
        luisa::filesystem::path &&path,
        uint64_t file_offset,
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level) override {
        auto render_device = RenderDevice::instance_ptr();
        if (!render_device) return;
        _path = std::move(path);
        _file_offset = file_offset;
        _size = size;
        _pixel_storage = pixel_storage;
        _mip_level = mip_level;
        if (!_device_image) {
            _device_image = new DeviceImage();
        } else {
            if (_device_image->loaded()) [[unlikely]] {
                LUISA_ERROR("Can not be create repeatly.");
            }
        }
        LUISA_ASSERT(host_data()->empty() || host_data()->size() == desire_size_bytes(), "Invalid host data length.");
        _device_image->create_texture<float>(
            render_device->lc_device(),
            (PixelStorage)pixel_storage,
            size,
            mip_level);
    }
    bool async_load_from_file() override {
        std::lock_guard lck{_async_mtx};
        auto render_device = RenderDevice::instance_ptr();
        if (!render_device) return false;
        auto file_size = desire_size_bytes();
        if (_path.empty()) {
            return false;
        }
        LUISA_ASSERT(host_data()->empty() || host_data()->size() == file_size, "Invalid host data length.");
        if (!_device_image) {
            _device_image = new DeviceImage();
        } else {
            if (_device_image->loaded()) [[unlikely]] {
                return false;
            }
        }
        _device_image->async_load_from_file(
            _path,
            _file_offset,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level,
            DeviceImage::ImageType::Float,
            !_device_image->host_data_ref().empty());
        return true;
    }
    void wait_load() const override {
        if (_device_image)
            _device_image->wait_finished();
    }
    void unload() override {
        _device_image.reset();
    }
    uint32_t heap_index() const override {
        if (!_device_image) return ~0u;
        _device_image->wait_executed();
        return _device_image->heap_idx();
    }
    bool loaded() const override {
        return _device_image && _device_image->loaded();
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Texture)

}// namespace rbc::world