#pragma once
#include <rbc_world_v2/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world_v2/type_register.h>
#include <stb/stb_image.h>
#include <tinytiffreader.h>
#include <tinyexr.h>

namespace rbc::world {
bool Texture::decode(luisa::filesystem::path const &path) {
    wait_load();
    if (loaded()) [[unlikely]] {
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
}// namespace rbc::world