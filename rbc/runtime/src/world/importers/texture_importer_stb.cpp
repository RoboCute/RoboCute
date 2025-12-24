#include <rbc_world/importers/texture_importer_stb.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/texture_loader.h>
#include <rbc_world/resource_importer.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/pack_texture.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <stb/stb_image.h>
#include <tinytiffreader.h>
#include <tinyexr.h>
#include <luisa/core/binary_io.h>
#include <rbc_world/type_register.h>
#include <algorithm>
#include <cctype>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool StbTextureImporter::can_import(luisa::filesystem::path const &path) const {
    auto ext = normalize_extension(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || 
           ext == ".bmp" || ext == ".gif" || ext == ".psd" || 
           ext == ".pnm" || ext == ".tga";
}

RC<TextureResource> StbTextureImporter::import(
    TextureLoader *loader,
    luisa::filesystem::path const &path,
    uint mip_level,
    bool to_vt) {
    
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return {};
    
    luisa::vector<std::byte> data;
    data.push_back_uninitialized(file_stream.length());
    file_stream.read(data);
    
    int x;
    int y;
    int channels_in_file;
    auto ptr = stbi_load_from_memory(
        (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
    
    if (!ptr) return {};
    
    auto tex = new DeviceImage();
    RC<TextureResource> result{world::create_object<TextureResource>()};
    
    ITextureImporter::tex_ref(result.get()) = tex;
    auto &img = tex->host_data_ref();
    ITextureImporter::size_ref(result.get()) = uint2(x, y);
    img.push_back_uninitialized(x * y * sizeof(stbi_uc) * 4);
    std::memcpy(img.data(), ptr, img.size_bytes());
    stbi_image_free(ptr);
    ITextureImporter::pixel_storage_ref(result.get()) = LCPixelStorage::BYTE4;
    ITextureImporter::mip_level_ref(result.get()) = 1;
    ITextureImporter::is_vt_ref(result.get()) = false;
    
    // Process texture (mip generation, VT conversion, etc.)
    // Note: process_texture will be called by TextureLoader::decode_texture
    // We don't call it here to avoid double processing
    
    return result;
}

RC<TextureResource> HdrTextureImporter::import(
    TextureLoader *loader,
    luisa::filesystem::path const &path,
    uint mip_level,
    bool to_vt) {
    
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return {};
    
    luisa::vector<std::byte> data;
    data.push_back_uninitialized(file_stream.length());
    file_stream.read(data);
    
    int x;
    int y;
    int channels_in_file;
    auto ptr = stbi_loadf_from_memory(
        (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
    
    if (!ptr) return {};
    
    auto tex = new DeviceImage();
    RC<TextureResource> result{world::create_object<TextureResource>()};
    
    ITextureImporter::tex_ref(result.get()) = tex;
    auto &img = tex->host_data_ref();
    ITextureImporter::size_ref(result.get()) = uint2(x, y);
    img.push_back_uninitialized(x * y * sizeof(float) * 4);
    std::memcpy(img.data(), ptr, img.size_bytes());
    stbi_image_free(ptr);
    ITextureImporter::pixel_storage_ref(result.get()) = LCPixelStorage::FLOAT4;
    ITextureImporter::mip_level_ref(result.get()) = 1;
    ITextureImporter::is_vt_ref(result.get()) = false;
    
    // Note: process_texture will be called by TextureLoader::decode_texture
    return result;
}

RC<TextureResource> TiffTextureImporter::import(
    TextureLoader *loader,
    luisa::filesystem::path const &path,
    uint mip_level,
    bool to_vt) {
    
    // Implementation from texture_loader.cpp
    auto tiffr = TinyTIFFReader_open(luisa::to_string(path).c_str());
    if (!tiffr) return {};
    auto dsp = vstd::scope_exit([&] {
        TinyTIFFReader_close(tiffr);
    });
    if (TinyTIFFReader_wasError(tiffr)) {
        LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
        return {};
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
            return {};
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
            return {};
    }
    RC<TextureResource> result{world::create_object<TextureResource>()};
    ITextureImporter::pixel_storage_ref(result.get()) = (LCPixelStorage)pixel_storage;
    ITextureImporter::size_ref(result.get()) = uint2(width, height);
    ITextureImporter::mip_level_ref(result.get()) = 1;
    ITextureImporter::is_vt_ref(result.get()) = false;
    luisa::vector<std::byte> image;
    image.push_back_uninitialized(width * height * bitspersample / 8);
    if (samples == 1) {
        TinyTIFFReader_getSampleData(tiffr, image.data(), 0);
        if (TinyTIFFReader_wasError(tiffr)) {
            LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
            return {};
        }
            auto tex = new DeviceImage();
            ITextureImporter::tex_ref(result.get()) = tex;
            tex->host_data_ref() = std::move(image);
        // Note: process_texture will be called by TextureLoader::decode_texture
        return result;
    }
        auto tex = new DeviceImage();
        ITextureImporter::tex_ref(result.get()) = tex;
        tex->host_data_ref().resize_uninitialized(pixel_storage_size(pixel_storage, make_uint3(ITextureImporter::size_ref(result.get()), 1)));
    auto pixel_count = width * height;
    for (uint16_t sample = 0; sample < samples; sample++) {
        TinyTIFFReader_getSampleData(tiffr, image.data(), sample);
            if (TinyTIFFReader_wasError(tiffr)) {
                LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
                ITextureImporter::tex_ref(result.get()).reset();
                return {};
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
                    ptr[i * channel_rate + 3] = 0;
                }
            } break;
            case 16: {
                auto ptr = (uint16_t *)tex->host_data_ref().data();
                for (uint i = 0; i < pixel_count; ++i) {
                    ptr[i * channel_rate + 3] = 0;
                }
            } break;
            default: {
                auto ptr = (uint32_t *)tex->host_data_ref().data();
                for (uint i = 0; i < pixel_count; ++i) {
                    ptr[i * channel_rate + 3] = 0;
                }
            } break;
        }
    }
    // Note: process_texture will be called by TextureLoader::decode_texture
    return result;
}

RC<TextureResource> ExrTextureImporter::import(
    TextureLoader *loader,
    luisa::filesystem::path const &path,
    uint mip_level,
    bool to_vt) {
    
    float *out;
    int width;
    int height;
    const char *err = nullptr;
    int ret = LoadEXR(&out, &width, &height, luisa::to_string(path).c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            LUISA_WARNING("{}", err);
            FreeEXRErrorMessage(err);
            return {};
        }
    }
    auto tex = new DeviceImage();
    RC<TextureResource> result{world::create_object<TextureResource>()};
    ITextureImporter::tex_ref(result.get()) = tex;
    auto &img = tex->host_data_ref();
    ITextureImporter::size_ref(result.get()) = make_uint2(width, height);
    img.push_back_uninitialized(width * height * sizeof(float) * 4);
    std::memcpy(img.data(), out, img.size_bytes());
    free(out);
    ITextureImporter::pixel_storage_ref(result.get()) = LCPixelStorage::FLOAT4;
    ITextureImporter::mip_level_ref(result.get()) = 1;
    ITextureImporter::is_vt_ref(result.get()) = false;
    
    // Note: process_texture will be called by TextureLoader::decode_texture
    return result;
}

}// namespace rbc::world

