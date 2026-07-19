#include <rbc_render/prepare_pass.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/utils/heitz_sobol.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/renderer_data.h>
#include <rbc_render/accum_pass.h>
#include <rbc_render/utils/color_space.h>
#include <luisa/core/binary_file_stream.h>
#include "spectrum_data.h"
#include <rbc_graphics/render_device.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <luisa/core/platform.h>
#include <luisa/core/stl/filesystem.h>
#include <rbc_render/click_manager.h>
#include <cmath>
#include <limits>
namespace rbc {
struct PreparePassContext : PassContext {
    Camera last_cam;
    float2 last_jitter;
    PreparePassContext(Camera const &cam)
        : last_cam(cam) {
    }
};
}// namespace rbc
RBC_RTTI(rbc::PreparePassContext);
namespace rbc {
namespace preparepass_detail {

template<class T, class G>
luisa::vector<T> generate_quantiles(
    luisa::span<T const> xs,
    luisa::span<T const> ys,
    G &&ps,
    T &integral) {
    LUISA_ASSERT(xs.size() == ys.size() && xs.size() >= 2u);
    const auto m = xs.size();
    luisa::vector<T> cdf(m);
    cdf[0] = T{0};
    for (size_t i = 1; i < m; ++i) {
        const auto dx = xs[i] - xs[i - 1u];
        cdf[i] = cdf[i - 1u] + (ys[i - 1u] + ys[i]) * dx * T{0.5};
    }
    integral = cdf.back();
    LUISA_ASSERT(integral > T{0});
    for (auto &v : cdf) {
        v /= integral;
    }

    size_t start = 0;
    while (start < m && ys[start] <= T{0}) {
        ++start;
    }
    size_t end = m - 1;
    while (end > 0 && ys[end] <= T{0}) {
        --end;
    }
    const auto support_start = start > 0u ? start - 1u : start;
    const auto support_end = end + 1u < m ? end + 1u : end;

    luisa::vector<T> quantiles;
    quantiles.reserve(std::size(ps));
    for (auto &&p : std::forward<G>(ps)) {
        if (p <= T{0}) {
            quantiles.emplace_back(xs[support_start]);
            continue;
        }
        if (p >= T{1}) {
            quantiles.emplace_back(xs[support_end]);
            continue;
        }
        const auto upper = std::upper_bound(cdf.begin(), cdf.end(), p);
        const auto k = std::min<size_t>(upper - cdf.begin(), m - 1u);
        const auto j = k - 1u;
        const auto dx = xs[k] - xs[j];
        const auto segment_area = (p - cdf[j]) * integral / dx;
        const auto y0 = ys[j];
        const auto dy = ys[k] - y0;
        T t;
        if (luisa::abs(dy) <= std::numeric_limits<T>::epsilon()) {
            t = segment_area / luisa::max(y0, std::numeric_limits<T>::min());
        } else {
            const auto discriminant = luisa::max(y0 * y0 + T{2} * dy * segment_area, T{0});
            t = T{2} * segment_area /
                luisa::max(y0 + luisa::sqrt(discriminant), std::numeric_limits<T>::min());
        }
        quantiles.emplace_back(luisa::lerp(xs[j], xs[k], luisa::clamp(t, T{0}, T{1})));
    }
    return quantiles;
}

[[nodiscard]] float3x3 to_float3x3(double3x3 const &m) noexcept {
    return make_float3x3(
        make_float3(m.cols[0]),
        make_float3(m.cols[1]),
        make_float3(m.cols[2]));
}

}// namespace preparepass_detail
void PreparePass::_load_rec2020_lut(Device &device, luisa::filesystem::path const &runtime_dir) {
    static constexpr auto lut3d_size = spectrum::spectrum_lut3d_res * spectrum::spectrum_lut3d_res * spectrum::spectrum_lut3d_res * 3ull * sizeof(float4);
    luisa::vector<std::byte> vec;
    vec.resize_uninitialized(lut3d_size);
    _lut_load_cmds.emplace_back(
        luisa::fiber::async([&device, runtime_dir, this, ptr = vec.data()]() {
            BinaryFileStream file_stream{luisa::to_string(runtime_dir / "rec2020.bytes")};
            LUISA_ASSERT(file_stream.length() == lut3d_size);
            file_stream.read({ptr, lut3d_size});
            spectrum_lut_3d = device.create_volume<float>(PixelStorage::FLOAT4, uint3(spectrum::spectrum_lut3d_res * 3, spectrum::spectrum_lut3d_res, spectrum::spectrum_lut3d_res));
        }),
        std::move(vec),
        &spectrum_lut_3d);
}

void PreparePass::_load_transmission_ggx_lut(Device &device, luisa::filesystem::path const &runtime_dir) {
    static const uint3 transmission_ggx_energy_size{32u};
    static const size_t transmission_ggx_energy_size_bytes = transmission_ggx_energy_size.x * transmission_ggx_energy_size.y * transmission_ggx_energy_size.z * sizeof(float4);
    luisa::vector<std::byte> vec;
    vec.resize_uninitialized(transmission_ggx_energy_size_bytes);
    _lut_load_cmds.emplace_back(
        luisa::fiber::async([&device, runtime_dir, this, ptr = vec.data()]() {
            BinaryFileStream file_stream{luisa::to_string(runtime_dir / "trans_ggx.bytes")};
            LUISA_ASSERT(file_stream.length() == transmission_ggx_energy_size_bytes);
            file_stream.read({ptr, transmission_ggx_energy_size_bytes});
            transmission_ggx_energy = device.create_volume<float>(PixelStorage::FLOAT4, transmission_ggx_energy_size);
        }),
        std::move(vec),
        &transmission_ggx_energy);
}

double PreparePass::_compute_spectrum_luts() {
    luisa::vector<double> probabilities;
    probabilities.reserve(spectrum::wavelength_lut_size);
    for (size_t i = 0; i < spectrum::wavelength_lut_size; ++i) {
        probabilities.emplace_back(double(i) / double(spectrum::wavelength_lut_size - 1u));
    }

    luisa::vector<double> wavelengths;
    luisa::vector<double3> cmf;
    wavelengths.reserve(spectrum::wavelength_pdf_table_size);
    cmf.reserve(spectrum::wavelength_pdf_table_size);
    for (size_t i = 0; i < spectrum::wavelength_pdf_table_size; ++i) {
        auto const &sample = test::spectrum::CIE_xyz_1931_2deg[i];
        wavelengths.emplace_back(double(sample.first));
        cmf.emplace_back(make_double3(sample.second));
    }

    double3 cmf_integral{0.0};
    double3 d65_white{0.0};
    const auto d65_start = test::spectrum::CIE_std_illum_D65[0].first;
    for (size_t i = 1; i < spectrum::wavelength_pdf_table_size; ++i) {
        const auto dx = wavelengths[i] - wavelengths[i - 1u];
        cmf_integral += (cmf[i - 1u] + cmf[i]) * (0.5 * dx);
        const auto d65_index0 = size_t(wavelengths[i - 1u]) - size_t(d65_start);
        const auto d65_index1 = size_t(wavelengths[i]) - size_t(d65_start);
        const auto e0 = double(test::spectrum::CIE_std_illum_D65[d65_index0].second);
        const auto e1 = double(test::spectrum::CIE_std_illum_D65[d65_index1].second);
        d65_white += (cmf[i - 1u] * e0 + cmf[i] * e1) * (0.5 * dx);
    }
    const auto d65_normalization = d65_white.y / cmf_integral.y;
    d65_white /= d65_white.y;
    const auto d65_xyY = Chromaticities::XYZ_to_xyY(d65_white);
    const auto d65_xy = make_double2(d65_xyY.x, d65_xyY.y);

    const Chromaticities rec2020_chromaticities{EColorSpace::Rec2020};
    const ColorSpace rec2020{
        rec2020_chromaticities.red,
        rec2020_chromaticities.green,
        rec2020_chromaticities.blue,
        d65_xy};
    const Chromaticities ap0_chromaticities{EColorSpace::ACES_AP0};
    const ColorSpace ap0_d65{
        ap0_chromaticities.red,
        ap0_chromaticities.green,
        ap0_chromaticities.blue,
        d65_xy};
    const std::array accumulation_to_xyz{
        double3x3::eye(1.0),
        ap0_d65.to_xyz};

    for (size_t space = 0; space < spectrum_accumulation_space_count; ++space) {
        auto &lut = _spectrum_lut_data[space];
        lut.resize(spectrum::wavelength_lut_size, make_float4(0.0f));
        const auto xyz_to_accumulation = inverse(accumulation_to_xyz[space]);
        luisa::vector<double3> responses;
        responses.reserve(spectrum::wavelength_pdf_table_size);
        for (const auto xyz : cmf) {
            const auto response = xyz_to_accumulation * xyz;
            LUISA_ASSERT(
                response.x >= -1e-9 && response.y >= -1e-9 && response.z >= -1e-9,
                "Spectrum accumulation basis {} has a negative wavelength response.",
                space);
            responses.emplace_back(double3{
                std::max(response.x, 0.0),
                std::max(response.y, 0.0),
                std::max(response.z, 0.0)});
        }

        double3 response_integral{0.0};
        luisa::vector<double> response;
        response.reserve(spectrum::wavelength_pdf_table_size);
        for (size_t channel = 0; channel < 3u; ++channel) {
            response.clear();
            for (const auto value : responses) {
                response.emplace_back(value[channel]);
            }
            double integral;
            const auto quantiles = preparepass_detail::generate_quantiles<double>(
                wavelengths, response, probabilities, integral);
            for (size_t i = 1; i < quantiles.size(); ++i) {
                LUISA_ASSERT(quantiles[i] >= quantiles[i - 1u]);
            }
            double normalized_pdf_integral = 0.0;
            for (size_t i = 1; i < response.size(); ++i) {
                const auto dx = wavelengths[i] - wavelengths[i - 1u];
                normalized_pdf_integral +=
                    (response[i - 1u] + response[i]) * (0.5 * dx / integral);
            }
            LUISA_ASSERT(std::abs(normalized_pdf_integral - 1.0) < 1e-9);
            response_integral[channel] = integral;
            for (size_t i = 0; i < spectrum::wavelength_lut_size; ++i) {
                lut[i][channel] = float(quantiles[i]);
            }
            for (size_t i = 0; i < spectrum::wavelength_pdf_table_size; ++i) {
                lut[channel * spectrum::wavelength_pdf_table_size + i].w =
                    float(response[i] / integral);
            }
        }

        const auto rec2020_to_accumulation = xyz_to_accumulation * rec2020.to_xyz;
        const auto accumulation_to_rec2020 = rec2020.from_xyz * accumulation_to_xyz[space];
        const auto round_trip = accumulation_to_rec2020 * rec2020_to_accumulation;
        for (size_t column = 0; column < 3u; ++column) {
            for (size_t row = 0; row < 3u; ++row) {
                const auto expected = column == row ? 1.0 : 0.0;
                LUISA_ASSERT(std::abs(round_trip.cols[column][row] - expected) < 1e-9);
            }
        }
        if (space == static_cast<size_t>(SpectrumAccumulationSpace::AP0D65)) {
            const auto neutral = xyz_to_accumulation * d65_white;
            LUISA_ASSERT(all(abs(neutral - double3{1.0}) < double3{1e-9}));
        }

        _spectrum_args[space] = SpectrumAccumulationArgs{
            .rec2020_to_accumulation = preparepass_detail::to_float3x3(
                rec2020_to_accumulation),
            .accumulation_to_rec2020 = preparepass_detail::to_float3x3(
                accumulation_to_rec2020),
            .lane_scale = make_float3(response_integral / cmf_integral.y)};
    }
    return d65_normalization;
}

luisa::vector<float> PreparePass::_compute_illum_d65_lut(double d65_normalization) {
    luisa::vector<float> illum_d65_lut_data;
    constexpr uint step = 10;
    const uint lut_resolution =
        uint(spectrum::wavelength_max - spectrum::wavelength_min) / step + 1u;
    illum_d65_lut_data.push_back_uninitialized(lut_resolution);
    auto const &start = test::spectrum::CIE_std_illum_D65[0];
    for (size_t i = 0; i < lut_resolution; ++i) {
        illum_d65_lut_data[i] = float(
            test::spectrum::CIE_std_illum_D65[
                size_t(spectrum::wavelength_min) - start.first + i * step]
                .second /
            d65_normalization);
    }
    LUISA_ASSERT(spectrum::illum_d65_size == lut_resolution);
    return illum_d65_lut_data;
}

void PreparePass::_initialize_sobol_resources(
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene,
    luisa::filesystem::path const &runtime_dir) {
    auto sobol_path = luisa::to_string(runtime_dir / "heitz_sobol.bytes");
    sobol_256d = heitz_sobol_256d(sobol_path, device, cmdlist, scene.after_commit_dsp_queue());
    sobol_scrambling = heitz_sobol_scrambling(sobol_path, device, cmdlist, scene.after_commit_dsp_queue(), HeitzSobolSPP::SPP256);
    sobol_ranking = heitz_sobol_ranking(sobol_path, device, cmdlist, scene.after_commit_dsp_queue(), HeitzSobolSPP::SPP256);
    scene.bindless_allocator().set_reserved_buffer(heap_indices::sobol_256d_heap_idx, sobol_256d);
    scene.bindless_allocator().set_reserved_buffer(heap_indices::sobol_scrambling_heap_idx, sobol_scrambling);
    scene.bindless_allocator().set_reserved_buffer(heap_indices::sobol_ranking_heap_idx, sobol_ranking);
}

void PreparePass::_create_and_upload_images(
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene,
    luisa::vector<float> &&illum_d65_lut_data) {
    spectrum_wavelength_lut = device.create_image<float>(
        PixelStorage::FLOAT4, make_uint2(spectrum::wavelength_lut_size, 1));
    const auto default_space = static_cast<uint>(SpectrumAccumulationSpace::AP0D65);
    _active_spectrum_accumulation_space = default_space;
    spectrum_args = _spectrum_args[default_space];
    auto &default_lut = _spectrum_lut_data[default_space];
    cmdlist << spectrum_wavelength_lut.copy_from(luisa::span(
        reinterpret_cast<std::byte *>(default_lut.data()), default_lut.size_bytes()));
    illum_d65 = device.create_image<float>(PixelStorage::FLOAT1, make_uint2(spectrum::illum_d65_size, 1));
    cmdlist << illum_d65.copy_from(luisa::span(reinterpret_cast<std::byte*>(illum_d65_lut_data.data()), illum_d65_lut_data.size_bytes()));
    scene.dispose_after_commit(std::move(illum_d65_lut_data));
}

void PreparePass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    auto const runtime_dir = RenderDevice::instance().lc_ctx().runtime_directory();
    
    _load_rec2020_lut(device, runtime_dir);
    _load_transmission_ggx_lut(device, runtime_dir);
    
    auto spectrum_future = luisa::fiber::async([this]() {
        return _compute_spectrum_luts();
    });

    _initialize_sobol_resources(device, cmdlist, scene, runtime_dir);

    const auto d65_normalization = spectrum_future.wait();
    auto illum_d65_lut_data = _compute_illum_d65_lut(d65_normalization);
    _create_and_upload_images(device, cmdlist, scene, std::move(illum_d65_lut_data));
}

void PreparePass::wait_enable() {
}

void PreparePass::_process_lut_load_commands(PipelineContext const &ctx) {
    for (auto &i : _lut_load_cmds) {
        i.evt.wait();
        (*ctx.cmdlist) << i.tex->copy_from(luisa::span(i.data));
        ctx.scene->dispose_after_commit(std::move(i.data));
    }
    _lut_load_cmds.clear();
}

void PreparePass::_update_spectrum_accumulation_space(PipelineContext const &ctx) {
    const auto &pt_settings = ctx.pipeline_settings.read<PathTracerSettings>();
    const auto spectrum_space = static_cast<uint>(pt_settings.spectrum_accumulation_space);
    LUISA_ASSERT(spectrum_space < spectrum_accumulation_space_count);
    spectrum_args = _spectrum_args[spectrum_space];
    if (_active_spectrum_accumulation_space == spectrum_space) {
        return;
    }

    auto &lut = _spectrum_lut_data[spectrum_space];
    (*ctx.cmdlist) << spectrum_wavelength_lut.copy_from(luisa::span(
        reinterpret_cast<std::byte *>(lut.data()), lut.size_bytes()));
    _active_spectrum_accumulation_space = spectrum_space;
    ctx.mut.get_pass_context<AccumPassContext>()->frame_index = 0;
}

void PreparePass::_update_camera_aspect_ratio(PipelineContext const &ctx, Camera &cam) {
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    cam.set_aspect_ratio_from_resolution(frame_settings.render_resolution.x, frame_settings.render_resolution.y);
}

void PreparePass::_initialize_first_frame(PipelineContext const &ctx, PreparePassContext *pass_ctx, Camera &cam) {
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    if (frame_settings.frame_index == 0) {
        pass_ctx->last_cam = cam;
    }
}

void PreparePass::_update_last_frame_camera_data(PipelineContext const &ctx, PreparePassContext *pass_ctx) {
    auto &jitter_data = ctx.pipeline_settings.read_mut<JitterData>();
    auto &cam_data = ctx.pipeline_settings.read_mut<CameraData>();

    jitter_data.last_jitter = pass_ctx->last_jitter;
    cam_data.last_proj = make_float4x4(pass_ctx->last_cam.projection_matrix());
    cam_data.last_view = make_float4x4(inverse(pass_ctx->last_cam.local_to_world_matrix()));
    cam_data.last_sky_view = make_float4x4(inverse(pass_ctx->last_cam.rotation_matrix()));
    cam_data.last_vp = cam_data.last_proj * cam_data.last_view;
    cam_data.last_sky_vp = cam_data.last_proj * cam_data.last_sky_view;
    cam_data.last_inv_vp = inverse(cam_data.last_vp);
}

void PreparePass::_set_color_space_matrix(PipelineContext const &ctx) {
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    switch (frame_settings.resource_color_space) {
        case ResourceColorSpace::Rec709:
            frame_settings.to_rec2020_matrix = make_float3x3(0.627404, 0.329283, 0.043313, 0.069097, 0.919540, 0.011362, 0.016391, 0.088013, 0.895595);
            break;
        case ResourceColorSpace::AdobeRGB:
            frame_settings.to_rec2020_matrix = make_float3x3(0.877334, 0.077494, 0.045172, 0.096623, 0.891527, 0.011850, 0.022921, 0.043037, 0.934042);
            break;
        case ResourceColorSpace::P3_D60:
            frame_settings.to_rec2020_matrix = make_float3x3(0.763906, 0.193206, 0.042888, 0.046355, 0.916212, 0.011251, -0.001227, 0.017124, 0.886810);
            break;
        case ResourceColorSpace::P3_D65:
            frame_settings.to_rec2020_matrix = make_float3x3(0.753833, 0.198597, 0.047570, 0.045744, 0.941777, 0.012479, -0.001210, 0.017602, 0.983609);
            break;
        case ResourceColorSpace::Rec2020:
            frame_settings.to_rec2020_matrix = float3x3::eye(1);
            break;
    }
    frame_settings.to_rec2020_matrix = transpose(frame_settings.to_rec2020_matrix);
}

void PreparePass::_bind_resources_to_heap(SceneManager &scene) {
    auto emplace_buffer = [&](uint idx, auto &&buffer) {
        if (buffer) {
            scene.bindless_allocator().set_reserved_buffer(idx, buffer);
        } else {
            scene.bindless_allocator().remove_reserved_buffer(idx);
        }
    };
    auto emplace_tex2d = [&](uint idx, auto &&image, Sampler sampler) {
        if (image) {
            scene.bindless_allocator().set_reserved_tex2d(idx, image, sampler);
        } else {
            scene.bindless_allocator().remove_reserved_tex2d(idx);
        }
    };
    auto emplace_tex3d = [&](uint idx, auto &&volume, Sampler sampler) {
        if (volume) {
            scene.bindless_allocator().set_reserved_tex3d(idx, volume, sampler);
        } else {
            scene.bindless_allocator().remove_reserved_tex3d(idx);
        }
    };
    emplace_buffer(heap_indices::light_bvh_heap_idx, scene.light_accel().tlas_buffer());
    emplace_buffer(heap_indices::area_lights_heap_idx, scene.light_accel().area_light_buffer());
    emplace_buffer(heap_indices::point_lights_heap_idx, scene.light_accel().point_light_buffer());
    emplace_buffer(heap_indices::spot_lights_heap_idx, scene.light_accel().spot_light_buffer());
    emplace_buffer(heap_indices::mesh_lights_heap_idx, scene.light_accel().mesh_light_buffer());
    emplace_buffer(heap_indices::disk_lights_heap_idx, scene.light_accel().disk_light_buffer());
    emplace_buffer(heap_indices::inst_buffer_heap_idx, scene.accel_manager().inst_buffer());
    emplace_buffer(heap_indices::procedural_type_buffer_idx, scene.accel_manager().procedural_type_buffer());
    emplace_tex3d(heap_indices::spectrum_lut3d_idx, spectrum_lut_3d, Sampler::linear_point_mirror());
    emplace_tex3d(heap_indices::transmission_ggx_energy_idx, transmission_ggx_energy, Sampler::linear_point_mirror());
    // emplace_tex3d(srgb_to_fourier_even_idx, srgb_to_fourier_even, Sampler::linear_point_mirror());
    // emplace_tex2d(bmese_phase_idx, bmese_phase, Sampler::linear_point_mirror());
    emplace_tex2d(heap_indices::illum_d65_idx, illum_d65, Sampler::linear_point_mirror());
    emplace_tex2d(
        heap_indices::spectrum_wavelength_lut_idx,
        spectrum_wavelength_lut,
        Sampler::linear_point_mirror());
}

void PreparePass::_update_current_frame_camera_data(PipelineContext const &ctx, Camera &cam) {
    auto &jitter_data = ctx.pipeline_settings.read_mut<JitterData>();
    auto &cam_data = ctx.pipeline_settings.read_mut<CameraData>();

    jitter_data.jitter = float2(0.f);
    cam_data.inv_view = make_float4x4(cam.local_to_world_matrix());
    cam_data.inv_sky_view = make_float4x4(cam.rotation_matrix());
    cam_data.view = inverse(cam_data.inv_view);
    cam_data.proj = make_float4x4(cam.projection_matrix());
    cam_data.vp = cam_data.proj * cam_data.view;
    cam_data.inv_proj = inverse(cam_data.proj);
    cam_data.inv_vp = inverse(cam_data.vp);
}

void PreparePass::_update_pass_context(PipelineContext const &ctx, PreparePassContext *pass_ctx, Camera &cam, bool is_first_frame) {
    auto &cam_data = ctx.pipeline_settings.read_mut<CameraData>();
    if (!is_first_frame) {
        pass_ctx->last_cam = cam;
    } else {
        cam_data.last_proj = cam_data.proj;
    }
}

void PreparePass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    _process_lut_load_commands(ctx);
    _update_spectrum_accumulation_space(ctx);

    auto &cam = ctx.pipeline_settings.read_mut<Camera>();
    auto pass_ctx = ctx.mut.get_pass_context<PreparePassContext>(cam);
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    bool is_first_frame = frame_settings.frame_index == 0;

    _update_camera_aspect_ratio(ctx, cam);
    _initialize_first_frame(ctx, pass_ctx, cam);
    _update_last_frame_camera_data(ctx, pass_ctx);
    _set_color_space_matrix(ctx);
    _bind_resources_to_heap(*ctx.scene);
    _update_current_frame_camera_data(ctx, cam);
    _update_pass_context(ctx, pass_ctx, cam, is_first_frame);
}
void PreparePass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto &cam = ctx.pipeline_settings.read_mut<Camera>();
    auto pass_ctx = ctx.mut.get_pass_context<PreparePassContext>(cam);
    auto &jitter_data = ctx.pipeline_settings.read_mut<JitterData>();
    pass_ctx->last_jitter = jitter_data.jitter;
}
void PreparePass::on_frame_end(
    Pipeline const &pipeline,
    Device &device,
    SceneManager &scene) {
}
void PreparePass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
}
PreparePass::~PreparePass() {
    for (auto &i : _lut_load_cmds) {
        i.evt.wait();
    }
    _lut_load_cmds.clear();
}
}// namespace rbc
