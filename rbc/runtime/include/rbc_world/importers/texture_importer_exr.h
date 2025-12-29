#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {
struct RBC_RUNTIME_API ExrTextureImporter final : ITextureImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".exr"; }
    [[nodiscard]] bool import(
        RC<TextureResource>,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) override;
};
}// namespace rbc::world