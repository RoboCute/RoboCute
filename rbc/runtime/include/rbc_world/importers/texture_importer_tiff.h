#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {
/**
 * @brief TIFF image importer for TextureResource
 */
struct RBC_RUNTIME_API TiffTextureImporter final : ITextureImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".tiff"; }
    [[nodiscard]] bool import(
        RC<TextureResource>,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};

}// namespace rbc::world