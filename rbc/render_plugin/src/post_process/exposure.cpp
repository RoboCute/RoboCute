#include <rbc_render/post_process/exposure.h>
#include <rbc_render/pipeline.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc {
namespace exposure_detail {
auto get_histogram_scale_offset_res(uint2 resolution) {
    auto diff = float(Exposure::range_max - Exposure::range_min);
    auto scale = 1.f / diff;
    auto offset = -float(Exposure::range_min) * scale;
    auto res = make_float2(resolution);
    res += float2(1e-4f);
    return float4(scale, offset, res.x, res.y);
}
void post_process_exposure(
    ExposureSettings const &desc,
    uint2 resolution,
    uint2 histogram_block_size,
    float4 &out_scale_offset_res,
    uint2 &out_histogram_disp_size,
    float4 &out_exposure_params1,
    float4 &out_exposure_params2) {
    out_scale_offset_res = get_histogram_scale_offset_res(resolution);
    ///////// Histogram
    auto dsp_size_flt = ceil(make_float2(resolution) / 2.0f);
    dsp_size_flt = ceil(dsp_size_flt / make_float2(histogram_block_size));
    out_histogram_disp_size = make_uint2(dsp_size_flt + float2(1e-4f)) * histogram_block_size;
    ///////// Exposure

    auto low_percent = desc.filtering.x;
    auto high_percent = desc.filtering.y;
    auto kMinDelta = 1e-2f;
    high_percent = max(high_percent, 1.0f + kMinDelta);
    low_percent = clamp(low_percent, 1.0f, high_percent - kMinDelta);

    // Clamp min/max adaptation values as well
    out_exposure_params1 = float4(low_percent * 0.01f, high_percent * 0.01f, exp2(desc.min_luminance), exp2(desc.max_luminance));
    out_exposure_params2 = float4(
        0, 0// desc.speedDown, desc.speedUp
        ,
        desc.global_exposure, 0.0f);
}
}// namespace exposure_detail
Exposure::Exposure(Device &device, luisa::fiber::counter &counter, uint2 res)
    : exposure_buffer(device.create_buffer<float>(4)) {
    ShaderManager::instance()->async_load(counter, "path_tracer/clear_buffer.bin", _clear_shader);
    ShaderManager::instance()->async_load(counter, "post_process/histogram.bin", _histogram_shader);
    ShaderManager::instance()->async_load(counter, "post_process/auto_exposure.bin", _auto_exposure);
    // local exposure
    // ShaderManager::instance()->load("post_process/write_log_lum_volume.bin", write_log_lum);
    // local_exp_volume = device.create_volume<float>(PixelStorage::FLOAT2, make_uint3((res + 15u) / 16u, 32u));
}

void Exposure::generate(
    ExposureSettings const &desc,
    CommandList &cmdlist,
    ImageView<float> img,
    uint2 res) {
    Buffer<uint> histogram_buffer = RenderDevice::instance().create_transient_buffer<uint>("histogram", k_Bins);
    ///////// Histogram
    float4 scale_offset_res;
    uint2 histogram_dispatch_size;
    float4 exposure_params1, exposure_params2;
    exposure_detail::post_process_exposure(desc, res, _histogram_shader->block_size().xy(), scale_offset_res, histogram_dispatch_size, exposure_params1, exposure_params2);
    cmdlist << (*_clear_shader)(histogram_buffer, 0).dispatch(histogram_buffer.size())
            << (*_histogram_shader)(
                   histogram_buffer,
                   img,
                   scale_offset_res)
                   .dispatch(histogram_dispatch_size);

    ///////// Exposure

    cmdlist << (*_auto_exposure)(
                   desc.use_auto_exposure ? -1.f : desc.global_exposure,
                   exposure_params1,
                   exposure_params2,
                   scale_offset_res,
                   histogram_buffer,
                   exposure_buffer)
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

Exposure::~Exposure() = default;
}// namespace rbc