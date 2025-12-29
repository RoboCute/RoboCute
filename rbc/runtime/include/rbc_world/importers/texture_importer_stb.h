#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

struct TextureLoader;

/**
 * @brief STB image importer for TextureResource (PNG, JPG, BMP, GIF, etc.)
 * Supports: .png, .jpg, .jpeg, .bmp, .gif, .psd, .pnm, .tga
 */
struct RBC_RUNTIME_API StbTextureImporter final : ITextureImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".png"; }
    [[nodiscard]] bool can_import(luisa::filesystem::path const &path) const override;
    [[nodiscard]] bool import(
        RC<TextureResource> resource,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

}// namespace rbc::world
