#include <rbc_graphics/texture/pack_texture.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {

PackTexture::PackTexture(Device &device) {
    ShaderManager::instance()->load("texture_process/bicubic_sample.bin", _gen_mip);
}

void PackTexture::generate_mip(
    CommandList &cmdlist,
    Image<float> const &img) {
    for (auto i : vstd::range(img.mip_levels() - 1)) {
        cmdlist << (*_gen_mip)(img.view(i), img.view(i + 1)).dispatch(img.view(i + 1).size());
    }
}

}// namespace rbc