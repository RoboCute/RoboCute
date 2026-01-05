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

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool StbTextureImporter::can_import(luisa::filesystem::path const &path) const {
    auto ext = normalize_extension(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".gif" || ext == ".psd" ||
           ext == ".pnm" || ext == ".tga";
}

bool StbTextureImporter::import(
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
    auto ptr = stbi_load_from_memory(
        (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);

    if (!ptr) return false;

    luisa::uint2 size{
        static_cast<unsigned int>(x),
        static_cast<unsigned int>(y)};
    resource->create_empty({}, LCPixelStorage::BYTE4, size, mip_level, to_vt);

    auto &img = resource->get_image()->host_data_ref();
    img.push_back_uninitialized(resource->desire_size_bytes());
    size_t image_size = x * y * sizeof(stbi_uc) * 4;
    std::memcpy(img.data(), ptr, image_size);
    stbi_image_free(ptr);

    loader->process_texture(resource, mip_level, to_vt);

    return true;
}

}// namespace rbc::world
