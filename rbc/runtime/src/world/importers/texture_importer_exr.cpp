#include "rbc_world/importers/texture_importer_exr.h"
#include "rbc_graphics/device_assets/device_image.h"
#include "rbc_world/resources/texture.h"
#include "rbc_world/texture_loader.h"
#include <tinyexr.h>

namespace rbc::world {

bool ExrTextureImporter::import(
    RC<TextureResource> resource,
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

    luisa::uint2 size{
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height)};
    mip_level = TextureResource::desired_mip_level(size, mip_level);
    resource->create_empty(LCPixelStorage::FLOAT4, size, mip_level, to_vt);

    auto &img = resource->get_image()->host_data_ref();
    img.clear();
    img.push_back_uninitialized(resource->desire_size_bytes());
    size_t image_size = width * height * sizeof(float) * 4;
    std::memcpy(img.data(), out, image_size);
    free(out);

    loader->process_texture(resource, mip_level, to_vt);

    return true;
}
}// namespace rbc::world