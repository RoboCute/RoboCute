#include <rbc_graphics/texture/pack_texture.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {

PackTexture::PackTexture(Device &device) {
    // to_tile_16byte = ShaderManager::instance()->load
    // ShaderManager::instance()->async_load(
    //     counter,
    //     "texture_process/to_tile_4bytes.bin", to_tile_4byte);
    // ShaderManager::instance()->async_load(
    //     counter,
    //     "texture_process/to_tile_8bytes.bin", to_tile_8byte);
    // ShaderManager::instance()->async_load(
    //     counter,
    //     "texture_process/to_tile_16bytes.bin", to_tile_16byte);
    ShaderManager::instance()->load("texture_process/bicubic_sample.bin", gen_mip);
}
PackTexture::~PackTexture() {}
// void PackTexture::pack_data(
//     CommandList &cmdlist,
//     BufferView<uint> src_data,
//     BufferView<uint> dst_data,
//     uint2 pack_tile,
//     uint2 tex_size) {
//     cmdlist << (*to_tile_4byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
// }
// void PackTexture::pack_data(
//     CommandList &cmdlist,
//     BufferView<uint2> src_data,
//     BufferView<uint2> dst_data,
//     uint2 pack_tile,
//     uint2 tex_size) {
//     cmdlist << (*to_tile_8byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
// }
// void PackTexture::pack_data(
//     CommandList &cmdlist,
//     BufferView<uint4> src_data,
//     BufferView<uint4> dst_data,
//     uint2 pack_tile,
//     uint2 tex_size) {
//     cmdlist << (*to_tile_16byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
// }
void PackTexture::generate_mip(
    CommandList &cmdlist,
    Image<float> const &img) {
    for (auto i : vstd::range(img.mip_levels() - 1)) {
        cmdlist << (*gen_mip)(img.view(i), img.view(i + 1)).dispatch(img.view(i + 1).size());
    }
}
}// namespace rbc