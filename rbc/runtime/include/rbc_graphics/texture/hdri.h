#pragma once
#include <rbc_io/io_service.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API HDRI {
private:
    Shader2D<
        Image<float>,
        Buffer<float>// scale_map,
        > const *_weight;

public:
    struct AliasEntry {
        float prob;
        uint alias;
    };
    struct AliasTable {
        luisa::vector<AliasEntry> table;
        luisa::vector<float> pdfs;
    };
    HDRI();
    void compute_scalemap(
        Device &device,
        CommandList &cmdlist,
        Image<float> const &img,
        uint2 size,
        Buffer<float> &buffer_cache,
        vstd::function<void(luisa::vector<float> &&)> callback);
    AliasTable compute_alias_table(luisa::span<float> scale_map, uint2 size);
    AliasTable precompute(
        Image<float> const &hdr,
        Device &device,
        Stream &stream);
};
}// namespace rbc
