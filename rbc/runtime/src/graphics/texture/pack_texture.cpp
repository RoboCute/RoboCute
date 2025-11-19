#include <rbc_graphics/texture/pack_texture.h>
#include <luisa/dsl/sugar.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc
{
namespace packtex_detail
{
template <typename T>
void to_tile(BufferVar<T> src_buffer, BufferVar<T> dst_buffer, UInt2 pack_tile)
{
    UInt2 coord        = dispatch_id().xy();
    UInt2 size         = dispatch_size().xy();
    UInt2 tile_size    = size / pack_tile;
    UInt2 tile_coord   = coord / pack_tile;
    UInt  tile_idx     = tile_coord.x + tile_coord.y * tile_size.x;
    UInt  pixel_offset = tile_idx * pack_tile.x * pack_tile.y;
    UInt2 inner_res    = coord % pack_tile;
    UInt  dst_idx      = pixel_offset + inner_res.x + inner_res.y * pack_tile.x;
    dst_buffer.write(dst_idx, src_buffer.read(coord.x + coord.y * size.x));
}
void gen_mip(ImageVar<float> src_img, ImageVar<float> dst_img)
{
    auto   id     = dispatch_id().xy();
    auto   src_id = id * 2u;
    Float4 v      = src_img.read(src_id) +
               src_img.read(src_id + uint2(1, 0)) +
               src_img.read(src_id + uint2(0, 1)) +
               src_img.read(src_id + uint2(1, 1));
    v *= 0.25f;
    dst_img.write(id, v);
}
} // namespace packtex_detail

PackTexture::PackTexture(Device& device)
{
    to_tile_16byte = ShaderManager::instance()->compile<2>(
        packtex_detail::to_tile<uint4>,
        ShaderOption{ .name = "to_tile_16bytes" }
    );
    to_tile_8byte = ShaderManager::instance()->compile<2>(
        packtex_detail::to_tile<uint2>,
        ShaderOption{ .name = "to_tile_8bytes" }
    );
    to_tile_4byte = ShaderManager::instance()->compile<2>(
        packtex_detail::to_tile<uint>,
        ShaderOption{ .name = "to_tile_4bytes" }
    );
    gen_mip = ShaderManager::instance()->compile<2>(
        packtex_detail::gen_mip,
        ShaderOption{ .name = "vt_gen_mip" }
    );
}
PackTexture::~PackTexture() {}
void PackTexture::pack_data(
    CommandList&     cmdlist,
    BufferView<uint> src_data,
    BufferView<uint> dst_data,
    uint2            pack_tile,
    uint2            tex_size
)
{
    cmdlist << (*to_tile_4byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
}
void PackTexture::pack_data(
    CommandList&      cmdlist,
    BufferView<uint2> src_data,
    BufferView<uint2> dst_data,
    uint2             pack_tile,
    uint2             tex_size
)
{
    cmdlist << (*to_tile_8byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
}
void PackTexture::pack_data(
    CommandList&      cmdlist,
    BufferView<uint4> src_data,
    BufferView<uint4> dst_data,
    uint2             pack_tile,
    uint2             tex_size
)
{
    cmdlist << (*to_tile_16byte)(src_data, dst_data, pack_tile).dispatch(tex_size);
}
void PackTexture::generate_mip(
    CommandList&        cmdlist,
    Image<float> const& img
)
{
    for (auto i : vstd::range(img.mip_levels() - 1))
    {
        cmdlist << (*gen_mip)(img.view(i), img.view(i + 1)).dispatch(img.view(i + 1).size());
    }
}
} // namespace rbc