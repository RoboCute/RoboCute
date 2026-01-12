#include "rbc_world/importers/texture_importer_tiff.h"
#include "tinytiffreader.h"
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_world/resources/texture.h>

namespace rbc::world {

bool TiffTextureImporter::import(
    RC<TextureResource> resource,
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

    RBC_UNIMPLEMENTED();

    // RC<TextureResource> result{world::create_object<TextureResource>()};
    // ITextureImporter::pixel_storage_ref(result.get()) = (LCPixelStorage)pixel_storage;
    // ITextureImporter::size_ref(result.get()) = uint2(width, height);
    // ITextureImporter::mip_level_ref(result.get()) = 1;
    // ITextureImporter::is_vt_ref(result.get()) = false;
    // luisa::vector<std::byte> image;
    // image.push_back_uninitialized(width * height * bitspersample / 8);
    // if (samples == 1) {
    //     TinyTIFFReader_getSampleData(tiffr, image.data(), 0);
    //     if (TinyTIFFReader_wasError(tiffr)) {
    //         LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
    //         return false;
    //     }
    //     auto tex = new DeviceImage();
    //     ITextureImporter::tex_ref(result.get()) = tex;
    //     tex->host_data_ref() = std::move(image);
    //     // Note: process_texture will be called by TextureLoader::decode_texture
    //     return result;
    // }
    // auto tex = new DeviceImage();
    // ITextureImporter::tex_ref(result.get()) = tex;
    // tex->host_data_ref().resize_uninitialized(pixel_storage_size(pixel_storage, make_uint3(ITextureImporter::size_ref(result.get()), 1)));
    // auto pixel_count = width * height;
    // for (uint16_t sample = 0; sample < samples; sample++) {
    //     TinyTIFFReader_getSampleData(tiffr, image.data(), sample);
    //     if (TinyTIFFReader_wasError(tiffr)) {
    //         LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
    //         ITextureImporter::tex_ref(result.get()).reset();
    //         return {};
    //     }
    //     switch (bitspersample) {
    //         case 8: {
    //             auto ptr = (uint8_t *)tex->host_data_ref().data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + sample] = (uint8_t)image[i];
    //             }
    //         } break;
    //         case 16: {
    //             auto ptr = (uint16_t *)tex->host_data_ref().data();
    //             auto image_ptr = (uint16_t *)image.data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + sample] = image_ptr[i];
    //             }
    //         } break;
    //         default: {
    //             auto ptr = (uint32_t *)tex->host_data_ref().data();
    //             auto image_ptr = (uint32_t *)image.data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + sample] = image_ptr[i];
    //             }
    //         } break;
    //     }
    // }
    // // make last zero
    // if (samples == 3) {
    //     switch (bitspersample) {
    //         case 8: {
    //             auto ptr = (uint8_t *)tex->host_data_ref().data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + 3] = 0;
    //             }
    //         } break;
    //         case 16: {
    //             auto ptr = (uint16_t *)tex->host_data_ref().data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + 3] = 0;
    //             }
    //         } break;
    //         default: {
    //             auto ptr = (uint32_t *)tex->host_data_ref().data();
    //             for (uint i = 0; i < pixel_count; ++i) {
    //                 ptr[i * channel_rate + 3] = 0;
    //             }
    //         } break;
    //     }
    // }

    return false;
}

}// namespace rbc::world