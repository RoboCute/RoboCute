#include <rbc_graphics/render_device.h>
#include <stb/stb_image_write.h>
#include <luisa/core/logging.h>
using namespace luisa::compute;
void save_image(luisa::filesystem::path const &path, Image<float> const &img) {
    auto &stream = rbc::RenderDevice::instance().lc_main_stream();
    LUISA_ASSERT(img.storage() == PixelStorage::BYTE4);
    luisa::vector<std::byte> bytes;
    bytes.push_back_uninitialized(img.view().size_bytes());
    stream << img.copy_to(bytes.data());
    if (path.has_parent_path() && !luisa::filesystem::is_directory(path.parent_path())) {
        std::error_code ec{};
        luisa::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            LUISA_WARNING("Illegal directory: {}", luisa::to_string(path));
        }
    }
    stream << synchronize();
    auto filename = luisa::to_string(path);
    stbi_write_jpg(
        filename.c_str(),
        img.size().x,
        img.size().y,
        4,
        bytes.data(),
        100);
    LUISA_INFO("{} saved", filename);
}