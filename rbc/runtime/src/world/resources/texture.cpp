#pragma once
#include <rbc_world/resources/texture.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world/type_register.h>
#include <rbc_world/resource_importer.h>

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
bool TextureResource::empty() const {
    return !_tex;
}
DeviceImage *TextureResource::get_image() const {
    std::shared_lock lck{_async_mtx};
    if (_tex && !is_vt()) {
        return static_cast<DeviceImage *>(_tex.get());
    }
    if (_is_vt) [[unlikely]]
        LUISA_ERROR("Getting non-sparse image from virtual texture");
    return nullptr;
}
DeviceSparseImage *TextureResource::get_sparse_image() const {
    std::shared_lock lck{_async_mtx};
    if (_tex && is_vt()) {
        return static_cast<DeviceSparseImage *>(_tex.get());
    }
    if (!_is_vt) [[unlikely]]
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
    obj.ar.value(_pixel_storage, "pixel_storage");
    obj.ar.value(_size, "size");
    obj.ar.value(_mip_level, "mip_level");
    obj.ar.value(is_vt(), "is_vt");
}
void TextureResource::deserialize_meta(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
#define RBC_TEXTURE_LOAD(m)        \
    {                              \
        decltype(_##m) m;          \
        if (obj.ar.value(m, #m)) { \
            _##m = m;              \
        }                          \
    }
    RBC_TEXTURE_LOAD(pixel_storage)
    RBC_TEXTURE_LOAD(mip_level)
    RBC_TEXTURE_LOAD(size)
    RBC_TEXTURE_LOAD(is_vt)
#undef RBC_TEXTURE_LOAD
}
void TextureResource::create_empty(
    LCPixelStorage pixel_storage,
    luisa::uint2 size,
    uint32_t mip_level,
    bool is_vt) {
    _status = EResourceLoadingStatus::Unloaded;
    if (_tex) [[unlikely]] {
        LUISA_ERROR("Can not create on exists texture.");
    }
    std::lock_guard lck{_async_mtx};
    _size = size;
    _pixel_storage = pixel_storage;
    _mip_level = mip_level;
    _is_vt = is_vt;
    if (is_vt) {
        _tex = new DeviceSparseImage();
        // _vt_finished = new VTLoadFlag{};
    } else {
        _tex = new DeviceImage();
    }
}
bool TextureResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_tex || _tex->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
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
    auto file_size = desire_size_bytes();
    auto host_data_ = host_data();
    LUISA_ASSERT(!host_data_ || host_data_->empty() || host_data_->size() == file_size, "Invalid host data length {}, requires {}.", host_data_->size(), file_size);
    std::lock_guard lck{_async_mtx};
    auto path = this->path();
    if (is_vt()) {
        if (path.empty()) {
            LUISA_WARNING("Virtual texture must have path.");
            return false;
        }
        auto tex = static_cast<DeviceSparseImage *>(_tex.get());
        tex->load(
            TexStreamManager::instance(),
            // [vt_finished = this->_vt_finished]() {
            //     vt_finished->finished = true;
            // },
            {},
            path,
            0,
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
    auto path = this->path();
    if (path.empty()) {
        return false;
    }
    std::lock_guard lck{_async_mtx};
    if (_tex) {
        return false;
    }
    if (is_vt()) {
        auto tex = new DeviceSparseImage();
        _tex = tex;
        // _vt_finished = new VTLoadFlag{};
        tex->load(
            TexStreamManager::instance(),
            // [vt_finished = this->_vt_finished]() {
            //     vt_finished->finished = true;
            // },
            {},
            path,
            0,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level);
    } else {
        auto tex = new DeviceImage();
        _tex = tex;
        tex->async_load_from_file(
            path,
            0,
            {},
            (PixelStorage)_pixel_storage,
            _size,
            _mip_level,
            DeviceImage::ImageType::Float,
            !tex->host_data_ref().empty());
    }
    return true;
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
    auto last_status = _status.load();
    _status = EResourceLoadingStatus::Loading;
    if (!_async_load_from_file()) {
        _status = last_status;
        co_return;
    }
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
        auto result = vt->load_executed();// && _vt_finished->finished;
        return result;
    } else {
        return static_cast<DeviceImage *>(_tex.get())->load_finished();
    }
}

void TextureResource::_pack_to_tile_level(uint level, luisa::span<std::byte const> src, luisa::span<std::byte> dst) {
    auto size = _size >> level;
    uint64_t pixel_size_bytes;
    uint64_t raw_size_bytes;
    uint chunk_resolution = TexStreamManager::chunk_resolution;
    if (is_block_compressed((PixelStorage)_pixel_storage)) {
        size /= 4u;
        pixel_size_bytes = pixel_storage_size((PixelStorage)_pixel_storage, uint3(4u, 4u, 1u));
        chunk_resolution /= 4;
    } else {
        pixel_size_bytes = pixel_storage_size((PixelStorage)_pixel_storage, uint3(1u));
    }
    raw_size_bytes = pixel_size_bytes * chunk_resolution;
    auto block_size_bytes = raw_size_bytes * chunk_resolution;
    auto block_count = (size + chunk_resolution - 1u) / chunk_resolution;
    auto get_src_offset = [&](uint2 coord) {
        return (coord.y * size.x + coord.x) * pixel_size_bytes;
    };
    auto get_dst_offset = [&](uint2 coord) {
        auto block_index = coord / chunk_resolution;
        auto block_offset = block_index.x + block_index.y * block_count.x;
        auto local_coord = (coord & (chunk_resolution - 1));
        return block_offset * block_size_bytes + pixel_size_bytes * (local_coord.x + local_coord.y * chunk_resolution);
    };

    for (auto block_x : vstd::range(block_count.x))
        for (auto block_y : vstd::range(block_count.y)) {
            for (auto raw : vstd::range(chunk_resolution)) {
                uint2 coord = uint2(block_x, block_y) * chunk_resolution + uint2(0, raw);
                const auto src_offset = get_src_offset(coord);
                const auto dst_offset = get_dst_offset(coord);
                std::memcpy(dst.data() + dst_offset, src.data() + src_offset, raw_size_bytes);
            }
        }
}
bool TextureResource::pack_to_tile() {
    if (_is_vt) return false;
    auto host_data_ptr = host_data();
    if (!host_data_ptr || host_data_ptr->empty()) return false;
    auto &host_data = *host_data_ptr;
    luisa::vector<std::byte> data;
    data.push_back_uninitialized(host_data.size());
    auto size = _size;
    uint64_t offset = 0;
    for (auto i : vstd::range(_mip_level)) {
        auto level_size = pixel_storage_size((PixelStorage)_pixel_storage, make_uint3(size, 1u));
        _pack_to_tile_level(
            i,
            luisa::span{host_data}.subspan(offset, level_size),
            luisa::span{data}.subspan(offset, level_size));
        size >>= 1u;
        offset += level_size;
    }
    host_data = std::move(data);
    save_to_path();
    _tex.reset();
    _tex = new DeviceSparseImage();
    // _vt_finished = new VTLoadFlag{};
    _is_vt = true;
    return true;
}

uint TextureResource::desired_mip_level(luisa::uint2 size, uint idx) {
    for (auto i : vstd::range(1, idx)) {
        size >>= 1u;
        if (any(size < 256u)) return i;
    }
    return idx;
}

bool TextureResource::decode(luisa::filesystem::path const &path, TextureLoader *tex_loader, uint mip_level, bool virtual_tex) {
    if (!empty()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists texture.");
        return false;
    }

    auto &registry = ResourceImporterRegistry::instance();
    auto *importer = registry.find_importer(path, ResourceType::Texture);
    if (!importer) {
        LUISA_WARNING("No importer found for texture file: {}", luisa::to_string(path));
        return false;
    }

    // Avoid dynamic_cast across DLL boundaries - use resource_type() check instead
    if (importer->resource_type() != ResourceType::Texture) {
        LUISA_WARNING("Invalid importer type for texture file: {}", luisa::to_string(path));
        return false;
    }
    auto *texture_importer = static_cast<ITextureImporter *>(importer);
    return texture_importer->import(RC<TextureResource>{this}, tex_loader, path, mip_level, virtual_tex);
}

DECLARE_WORLD_OBJECT_REGISTER(TextureResource)

}// namespace rbc::world