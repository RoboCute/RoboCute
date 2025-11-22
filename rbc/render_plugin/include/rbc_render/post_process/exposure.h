#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/shader.h>
#include <rbc_render/generated/pipeline_settings.hpp>
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct Exposure {
private:
    Shader2D<
        Buffer<uint>, //& histogram_buffer,
        Image<float>,
        float4 // scale_offset_res>
        > const* _histogram_shader;

    Shader1D<Buffer<uint>, uint> const* _clear_shader;

    Shader2D<
        float,        // exposure
        float4,       // _Params1,	   // x: lowPercent, y: highPercent, z: minBrightness, w: maxBrightness
        float4,       //_Params2,	   // x: speed down, y: speed up, z: exposure compensation, w: delta time
        float4,       //_ScaleOffsetRes,// x: scale, y: offset, w: histogram pass width, h: histogram pass height
        bool,         // progressive,
        Buffer<uint>, //& _HistogramBuffer,
        Buffer<float> //& SrcDst
        > const* _auto_exposure;
    luisa::fixed_vector<float, 4> _global_data;

public:
    static constexpr int rangeMin = -9; // ev
    static constexpr int rangeMax = 9;  // ev
    static constexpr int k_Bins = 128;

    Buffer<float> exposure_buffer;
    // Volume<float> local_exp_volume;

    Exposure(Device& device, luisa::fiber::counter& counter, uint2 res);
    void generate(
        ExposureSettings const& desc,
        CommandList& cmdlist,
        ImageView<float> img,
        uint2 res,
        bool reset,
        float delta_time
    );
    ~Exposure();
};
} // namespace rbc