#pragma once
#include <rbc_world_v2/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world_v2/type_register.h>
#include <stb/stb_image.h>

namespace rbc::world {
bool Texture::decode(luisa::filesystem::path const &path) {
    std::lock_guard lck{_async_mtx};
    wait_load();
    if (loaded()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists texture.");
        return false;
    }
    auto ext = luisa::to_string(path.extension());
    for (auto &i : ext) {
        i = std::tolower(i);
    }
    BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return false;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".psd" || ext == ".pnm" || ext == ".tga") {
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        int x;
        int y;
        int channels_in_file;
        auto ptr = stbi_load_from_memory((const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
        auto tex = new DeviceImage();
        _tex = tex;
        auto &img = tex->host_data_ref();
        _size = uint2(x, y);
        img.push_back_uninitialized(x * y * sizeof(stbi_uc) * 4);
        std::memcpy(img.data(), ptr, img.size_bytes());
        stbi_image_free(ptr);
        _pixel_storage = LCPixelStorage::BYTE4;
        _mip_level = 1; // TODO: generate mip
        _is_vt = false; // TODO: support virtual texture
    }
    return true;
}
}// namespace rbc::world