#include <rbc_render/post_process/exposure.h>
#include <rbc_render/pipeline.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc
{
namespace exposure_detail
{
const int rangeMin = -9; // ev
const int rangeMax = 9;  // ev
auto GetHistogramScaleOffsetRes(uint2 resolution)
{
    auto diff = float(rangeMax - rangeMin);
    auto scale = 1.f / diff;
    auto offset = -float(rangeMin) * scale;
    auto res = make_float2(resolution);
    res += float2(1e-4f);
    return float4(scale, offset, res.x, res.y);
}
auto post_process_exposure(
    ExposureSettings const& desc,
    uint2 resolution,
    uint2 histogram_block_size,
    float delta_time,
    float4& out_scale_offset_res,
    uint2& out_histogram_disp_size,
    float4& out_exposure_Params1,
    float4& out_exposure_Params2
)
{
    out_scale_offset_res = GetHistogramScaleOffsetRes(resolution);
    ///////// Histogram
    auto dsp_size_flt = ceil(make_float2(resolution) / 2.0f);
    dsp_size_flt = ceil(dsp_size_flt / make_float2(histogram_block_size));
    out_histogram_disp_size = make_uint2(dsp_size_flt + float2(1e-4f)) * histogram_block_size;
    ///////// Exposure

    auto lowPercent = desc.filtering.x;
    auto highPercent = desc.filtering.y;
    auto kMinDelta = 1e-2f;
    highPercent = max(highPercent, 1.0f + kMinDelta);
    lowPercent = clamp(lowPercent, 1.0f, highPercent - kMinDelta);

    // Clamp min/max adaptation values as well
    out_exposure_Params1 = float4(lowPercent * 0.01f, highPercent * 0.01f, exp2(desc.minLuminance), exp2(desc.maxLuminance));
    out_exposure_Params2 = float4(desc.speedDown, desc.speedUp, desc.globalExposure, delta_time);
}
} // namespace exposure_detail
Exposure::Exposure(Device& device, uint2 res)
     :exposure_buffer(device.create_buffer<float>(4))
{
    ShaderManager::instance()->load("path_tracer/clear_buffer.bin", _clear_shader);
    ShaderManager::instance()->load("post_process/histogram.bin", _histogram_shader);
    ShaderManager::instance()->load("post_process/auto_exposure.bin", _auto_exposure);
    // local exposure
    // ShaderManager::instance()->load("post_process/write_log_lum_volume.bin", write_log_lum);
    // local_exp_volume = device.create_volume<float>(PixelStorage::FLOAT2, make_uint3((res + 15u) / 16u, 32u));
}

void Exposure::generate(
    ExposureSettings const& desc,
    CommandList& cmdlist,
    ImageView<float> img,
    uint2 res,
    bool reset,
    float delta_time
)
{
    Buffer<uint> histogram_buffer = RenderDevice::instance().create_transient_buffer<uint>("histogram", k_Bins);
    ///////// Histogram
    float4 scaleOffsetRes;
    uint2 histogram_dispatch_size;
    float4 exposure_Params1, exposure_Params2;
    exposure_detail::post_process_exposure(desc, res, _histogram_shader->block_size().xy(), delta_time, scaleOffsetRes, histogram_dispatch_size, exposure_Params1, exposure_Params2);
    cmdlist << (*_clear_shader)(histogram_buffer, 0).dispatch(histogram_buffer.size())
            << (*_histogram_shader)(
                   histogram_buffer,
                   img,
                   scaleOffsetRes
               )
                   .dispatch(histogram_dispatch_size);

    ///////// Exposure

    cmdlist << (*_auto_exposure)(
                   desc.use_auto_exposure ? -1.f : desc.globalExposure,
                   exposure_Params1,
                   exposure_Params2,
                   scaleOffsetRes,
                   !reset,
                   histogram_buffer,
                   exposure_buffer
    )
                   .dispatch(_auto_exposure->block_size().xy());
    ///////// Local exposure
    // cmdlist << (*write_log_lum)(
    // 			   img,
    // 			   local_exp_volume,
    // 			   exposure_buffer,
    // 			   res)
    // 			   .dispatch(local_exp_volume.size().xy() * 16u);
    // auto temp_vol = temp_res.get_volume<float>(PixelStorage::FLOAT2, local_exp_volume.size());
    // cmdlist << (*blur_log_lum)(
    // 			   local_exp_volume,
    // 			   temp_vol.volume(),
    // 			   int2(1, 0),
    // 			   4,
    // 			   0.5f)
    // 			   .dispatch(local_exp_volume.size());
    // cmdlist << (*blur_log_lum)(
    // 			   temp_vol.volume(),
    // 			   local_exp_volume,
    // 			   int2(0, 1),
    // 			   4,
    // 			   0.5f)
    // 			   .dispatch(local_exp_volume.size());
}

Exposure::~Exposure() {}
} // namespace rbc