#include "rbc_world/importers/texture_importer_hdr.h"
#include "rbc_world/resources/texture.h"
#include "rbc_world/texture_loader.h"
#include <rbc_graphics/device_assets/device_image.h>
#include <stb/stb_image.h>

namespace rbc::world {

bool HdrTextureImporter::import(
    RC<TextureResource> resource,
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

    if (!ptr) return false;

    luisa::uint2 size{
        static_cast<unsigned int>(x),
        static_cast<unsigned int>(y)};
    mip_level = TextureResource::desired_mip_level(size, mip_level);
    resource->create_empty(LCPixelStorage::BYTE4, size, mip_level, to_vt);

    auto &img = resource->get_image()->host_data_ref();
    img.clear();
    img.push_back_uninitialized(resource->desire_size_bytes());
    std::memcpy(img.data(), ptr, img.size_bytes());

    stbi_image_free(ptr);

    loader->process_texture(resource, mip_level, to_vt);

    return false;
}

}// namespace rbc::world