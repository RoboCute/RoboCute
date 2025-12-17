#pragma once
#include <rbc_world_v2/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world_v2/type_register.h>
#include <stb/stb_image.h>
#include <tinytiffreader.h>
#include <tinyexr.h>

namespace rbc::world {
bool Texture::decode(luisa::filesystem::path const &path) {
    if (!empty()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists texture.");
        return false;
    }
    auto ext = luisa::to_string(path.extension());
    for (auto &i : ext) {
        i = std::tolower(i);
    }
    if (!luisa::filesystem::exists(path)) return false;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".psd" || ext == ".pnm" || ext == ".tga") {
        BinaryFileStream file_stream(luisa::to_string(path));
        if (!file_stream.valid()) return false;
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        int x;
        int y;
        int channels_in_file;
        auto ptr = stbi_load_from_memory(
            (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
        auto tex = new DeviceImage();
        _tex = tex;
        auto &img = tex->host_data_ref();
        _size = uint2(x, y);
        img.push_back_uninitialized(x * y * sizeof(stbi_uc) * 4);
        std::memcpy(img.data(), ptr, img.size_bytes());
        stbi_image_free(ptr);
        _pixel_storage = LCPixelStorage::BYTE4;
        _mip_level = 1;// TODO: generate mip
        _is_vt = false;// TODO: support virtual texture
        return true;
    }
    if (ext == ".hdr") {
        BinaryFileStream file_stream(luisa::to_string(path));
        if (!file_stream.valid()) return false;
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        int x;
        int y;
        int channels_in_file;
        auto ptr = stbi_loadf_from_memory(
            (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
        auto tex = new DeviceImage();
        _tex = tex;
        auto &img = tex->host_data_ref();
        _size = uint2(x, y);
        img.push_back_uninitialized(x * y * sizeof(float) * 4);
        std::memcpy(img.data(), ptr, img.size_bytes());
        stbi_image_free(ptr);
        _pixel_storage = LCPixelStorage::FLOAT4;
        _mip_level = 1;// TODO: generate mip
        _is_vt = false;// TODO: support virtual texture
        return true;
    }
    if (ext == ".tiff") {
        auto tiffr = TinyTIFFReader_open(luisa::to_string(path).c_str());
        if (!tiffr) return false;
        auto dsp = vstd::scope_exit([&] {
            TinyTIFFReader_close(tiffr);
        });
        if (TinyTIFFReader_wasError(tiffr)) {
            LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
            return false;
        }
        const uint16_t samples = TinyTIFFReader_getSamplesPerPixel(tiffr);
        const uint16_t bitspersample = TinyTIFFReader_getBitsPerSample(tiffr, 0);
        auto width = TinyTIFFReader_getWidth(tiffr);
        auto height = TinyTIFFReader_getHeight(tiffr);
        PixelStorage pixel_storage;
        switch (bitspersample) {
            case 8:
                pixel_storage = PixelStorage::BYTE1;
                break;
            case 16:
                pixel_storage = PixelStorage::SHORT1;
                break;
            case 32:
                pixel_storage = PixelStorage::INT1;
                break;
            default:
                LUISA_WARNING("Unsupported bits-per-sample");
                return false;
        }
        uint channel_rate = 1;
        switch (samples) {
            case 1:
                break;
            case 2:
                pixel_storage = (PixelStorage)(luisa::to_underlying(pixel_storage) + 1);
                channel_rate = 2;
                break;
            case 3:
            case 4:
                pixel_storage = (PixelStorage)(luisa::to_underlying(pixel_storage) + 2);
                channel_rate = 4;
                break;
            default:
                LUISA_WARNING("Unsupported sample count");
                return false;
        }
        _pixel_storage = (LCPixelStorage)pixel_storage;
        _size = uint2(width, height);
        _mip_level = 1;// TODO: mipmap generate
        _is_vt = false;
        luisa::vector<std::byte> image;
        image.push_back_uninitialized(width * height * bitspersample / 8);
        if (samples == 1) {
            TinyTIFFReader_getSampleData(tiffr, image.data(), 0);
            if (TinyTIFFReader_wasError(tiffr)) {
                LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
                return false;
            }
            auto tex = new DeviceImage();
            _tex = tex;
            tex->host_data_ref() = std::move(image);
            return true;
        }
        auto tex = new DeviceImage();
        _tex = tex;
        tex->host_data_ref().resize_uninitialized(pixel_storage_size(pixel_storage, make_uint3(_size, 1)));
        auto pixel_count = width * height;
        for (uint16_t sample = 0; sample < samples; sample++) {
            // read the sample
            TinyTIFFReader_getSampleData(tiffr, image.data(), sample);
            if (TinyTIFFReader_wasError(tiffr)) {
                LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
                _tex.reset();
                return false;
            }
            switch (bitspersample) {
                case 8: {
                    auto ptr = (uint8_t *)tex->host_data_ref().data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = (uint8_t)image[i];
                    }
                } break;
                case 16: {
                    auto ptr = (uint16_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint16_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = image_ptr[i];
                    }
                } break;
                default: {
                    auto ptr = (uint32_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint32_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = image_ptr[i];
                    }
                } break;
            }
        }
        // make last zero
        if (samples == 3) {
            switch (bitspersample) {
                case 8: {
                    auto ptr = (uint8_t *)tex->host_data_ref().data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = (uint8_t)image[i];
                    }
                } break;
                case 16: {
                    auto ptr = (uint16_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint16_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = image_ptr[i];
                    }
                } break;
                default: {
                    auto ptr = (uint32_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint32_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = image_ptr[i];
                    }
                } break;
            }
        }
        return true;
    }
    if (ext == ".exr") {
        float *out;// width * height * RGBA
        int width;
        int height;
        const char *err = nullptr;
        int ret = LoadEXR(&out, &width, &height, luisa::to_string(path).c_str(), &err);
        if (ret != TINYEXR_SUCCESS) {
            if (err) {
                LUISA_WARNING("{}", err);
                FreeEXRErrorMessage(err);// release memory of error message.
                return false;
            }
        }
        auto tex = new DeviceImage();
        _tex = tex;
        auto &img = tex->host_data_ref();
        _size = make_uint2(width, height);
        img.push_back_uninitialized(width * height * sizeof(float) * 4);
        std::memcpy(img.data(), out, img.size_bytes());
        free(out);
        _pixel_storage = LCPixelStorage::FLOAT4;
        _mip_level = 1;// TODO: generate mip
        _is_vt = false;// TODO: support virtual texture
        return true;
    }
    return false;
}

void Texture::_pack_to_tile_level(uint level, luisa::span<std::byte const> src, luisa::span<std::byte> dst) {
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
bool Texture::pack_to_tile() {
    if (_is_vt) return false;
    auto host_data_ = host_data();
    if (!host_data_ || host_data_->empty()) return false;
    luisa::vector<std::byte> data;
    data.push_back_uninitialized(host_data_->size());
    auto size = _size;
    uint64_t offset = 0;
    for (auto i : vstd::range(_mip_level)) {
        auto level_size = pixel_storage_size((PixelStorage)_pixel_storage, make_uint3(size, 1u));
        _pack_to_tile_level(
            i,
            luisa::span{*host_data_}.subspan(offset, level_size),
            luisa::span{data}.subspan(offset, level_size));
        size >>= 1u;
        offset += level_size;
    }
    *host_data_ = std::move(data);
    _is_vt = true;
    return true;
}
}// namespace rbc::world