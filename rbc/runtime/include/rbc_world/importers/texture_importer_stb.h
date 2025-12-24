#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

struct TextureLoader;

/**
 * @brief STB image importer for TextureResource (PNG, JPG, BMP, GIF, etc.)
 * Supports: .png, .jpg, .jpeg, .bmp, .gif, .psd, .pnm, .tga
 */
struct RBC_RUNTIME_API StbTextureImporter final : ITextureImporter {
    luisa::string_view extension() const override { return ".png"; }
    
    bool can_import(luisa::filesystem::path const &path) const override;
    
    RC<TextureResource> import(
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

/**
 * @brief HDR image importer for TextureResource
 */
struct RBC_RUNTIME_API HdrTextureImporter final : ITextureImporter {
    luisa::string_view extension() const override { return ".hdr"; }
    
    RC<TextureResource> import(
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

/**
 * @brief TIFF image importer for TextureResource
 */
struct RBC_RUNTIME_API TiffTextureImporter final : ITextureImporter {
    luisa::string_view extension() const override { return ".tiff"; }
    
    RC<TextureResource> import(
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

/**
 * @brief EXR image importer for TextureResource
 */
struct RBC_RUNTIME_API ExrTextureImporter final : ITextureImporter {
    luisa::string_view extension() const override { return ".exr"; }
    
    RC<TextureResource> import(
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

}// namespace rbc::world

