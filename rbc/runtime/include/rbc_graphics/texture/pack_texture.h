#pragma once
#include <rbc_config.h>
#include <luisa/runtime/shader.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct RBC_RUNTIME_API PackTexture {
private:
    template<typename T>
    using ToTileShader = Shader2D<Buffer<T>, Buffer<T>, uint2>;
    ToTileShader<uint4> const *to_tile_16byte;
    ToTileShader<uint2> const *to_tile_8byte;
    ToTileShader<uint> const *to_tile_4byte;
    Shader2D<Image<float>, Image<float>> const *gen_mip;

public:
    PackTexture(Device &device);
    ~PackTexture();
    void generate_mip(
        CommandList &cmdlist,
        Image<float> const &img);
    // currently working in host side

    // void pack_data(
    //     CommandList &cmdlist,
    //     BufferView<uint> src_data,
    //     BufferView<uint> dst_data,
    //     uint2 pack_tile,
    //     uint2 tex_size);
    // void pack_data(
    //     CommandList &cmdlist,
    //     BufferView<uint2> src_data,
    //     BufferView<uint2> dst_data,
    //     uint2 pack_tile,
    //     uint2 tex_size);
    // void pack_data(
    //     CommandList &cmdlist,
    //     BufferView<uint4> src_data,
    //     BufferView<uint4> dst_data,
    //     uint2 pack_tile,
    //     uint2 tex_size);
};
}// namespace rbc