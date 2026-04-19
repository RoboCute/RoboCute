#pragma once
#include <rbc_config.h>
#include <luisa/runtime/shader.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct RBC_RUNTIME_API PackTexture {
private:
    Shader2D<Image<float>, Image<float>> const *_gen_mip;

public:
    PackTexture(Device &device);
    ~PackTexture() = default;
    void generate_mip(
        CommandList &cmdlist,
        Image<float> const &img);
};
}// namespace rbc