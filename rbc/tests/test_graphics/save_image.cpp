#include <rbc_graphics/render_device.h>
#include <stb/stb_image_write.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
using namespace luisa::compute;
void save_image(luisa::filesystem::path const &path, Image<float> const &img) {
    auto &stream = rbc::RenderDevice::instance().lc_main_stream();
    luisa::vector<std::byte> bytes;
    bytes.push_back_uninitialized(pixel_storage_size(PixelStorage::BYTE4, make_uint3(img.size(), 1)));
    if (img.storage() == PixelStorage::BYTE4) {
        stream << img.copy_to(bytes.data())
               << synchronize();
    } else {
        CommandList cmdlist;
        Image<float> temp_img = rbc::RenderDevice::instance().lc_device().create_image<float>(PixelStorage::BYTE4, img.size());
        rbc::SceneManager::instance().tex_uploader().blit(
            cmdlist,
            img,
            temp_img,
            luisa::float2(1),
            luisa::float2(),
            luisa::uint2(),
            img.size());
        cmdlist << temp_img.copy_to(bytes.data());
        stream << cmdlist.commit()
               << synchronize();
    }
    if (path.has_parent_path() && !luisa::filesystem::is_directory(path.parent_path())) {
        std::error_code ec{};
        luisa::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            LUISA_WARNING("Illegal directory: {}", luisa::to_string(path));
        }
    }
    auto filename = luisa::to_string(path);
    auto ext = luisa::to_string(path.extension());
    for (auto &i : ext) {
        i = std::tolower(i);
    }
    if (ext == ".png") {
        stbi_write_png(
            filename.c_str(),
            img.size().x,
            img.size().y,
            4,
            bytes.data(),
            img.size().x * 4);
    } else {
        // JPEG only supports 3 channels (RGB), stbi_write_jpg does not support RGBA
        LUISA_WARNING("JPEG format does not support alpha channel, saving as PNG instead");
        stbi_write_jpg(
            filename.c_str(),
            img.size().x,
            img.size().y,
            4,
            bytes.data(),
            100);
    }
    LUISA_INFO("{} saved", filename);
}