#include <rbc_render/prepare_pass.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/utils/heitz_sobol.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/renderer_data.h>
#include <luisa/core/binary_file_stream.h>
#include "spectrum_data.h"
#include <spectrum/spectrum_args.hpp>
#include <rbc_graphics/render_device.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <luisa/core/platform.h>
#include <luisa/core/stl/filesystem.h>
namespace rbc
{
struct PreparePassContext : PassContext {
    Camera last_cam;
    PreparePassContext(Camera const& cam)
        : last_cam(cam)
    {
    }
};
} // namespace rbc
RBC_RTTI(rbc::PreparePassContext);
namespace rbc
{
namespace preparepass_detail
{

/**
 * Compute inverse CDF (quantiles) from unnormalized PDF.
 *
 * @param x_span Sorted span of x values (size m).
 * @param y_span PDF values corresponding to x (size m), may include zeros at ends.
 * @param ps probabilities in (0,1) for which to compute quantiles.
 * @param iterations Maximum Newton-Raphson iterations per quantile.
 * @param tol Convergence tolerance for Newton-Raphson.
 * @return Vector of quantiles corresponding to probabilities ps.
 */
template <class T, class G>
luisa::vector<T> generate_quantiles(
    luisa::span<T const> xs,
    luisa::span<T const> ys,
    G&& ps,
    int iterations,
    T tol
)
{
    size_t m = xs.size();

    // Compute integration step sizes (left differences)
    luisa::vector<T> dx(m);
    dx[0] = 0.0;
    for (size_t i = 1; i < m; ++i)
    {
        dx[i] = xs[i] - xs[i - 1];
    }

    // Compute unnormalized CDF
    luisa::vector<T> cdf(m);
    cdf[0] = ys[0] * dx[0];
    for (size_t i = 1; i < m; ++i)
    {
        cdf[i] = cdf[i - 1] + ys[i] * dx[i];
    }
    auto& total = cdf.back();
    for (auto& v : cdf)
        v /= total; // Normalize to [0,1]

    // Determine valid support where PDF > 0
    size_t start = 0;
    while (start < m && ys[start] <= 0)
        ++start;
    size_t end = m - 1;
    while (end > 0 && ys[end] <= 0)
        --end;
    auto& xmin = xs[start];
    auto& xmax = xs[end];

    // Prepare output
    luisa::vector<T> quantiles;

    // Inverse CDF for each probability in ps
    for (auto&& p : std::forward<G>(ps))
    {
        // Initial guess via linear interpolation on CDF
        auto it = std::lower_bound(cdf.begin(), cdf.end(), p);
        T x0;
        if (it == cdf.begin())
        {
            x0 = xs[0];
        }
        else if (it == cdf.end())
        {
            x0 = xs[m - 1];
        }
        else
        {
            size_t j = it - cdf.begin();
            auto& c1 = cdf[j - 1];
            auto& c2 = cdf[j];
            auto t = (p - c1) / (c2 - c1);
            x0 = xs[j - 1] + t * (xs[j] - xs[j - 1]);
        }

        // Newton-Raphson refinement
        for (int iters = 0; iters < iterations; ++iters)
        {
            // Locate interval for x0
            auto up = std::upper_bound(xs.begin(), xs.begin() + m, x0);
            size_t j = (up == xs.begin()) ? 0 : (up - xs.begin() - 1);
            size_t k = luisa::min(j + 1, m - 1);

            auto xj = xs[j], xk = xs[k];
            auto cj = cdf[j], ck = cdf[k];
            auto yj = ys[j], yk = ys[k];
            auto t = (xk == xj ? 0 : (x0 - xj) / (xk - xj));
            auto cval = cj + t * (ck - cj);
            auto pdf = yj + t * (yk - yj);

            if (pdf <= 0) break;
            auto xnew = x0 - (cval - p) / pdf;
            if (luisa::abs(xnew - x0) < tol)
            {
                x0 = xnew;
                break;
            }
            x0 = xnew;
        }

        // Clamp to valid support
        quantiles.emplace_back(luisa::clamp(x0, xmin, xmax));
    }

    return quantiles;
}

float4x4 inverse_double(float4x4 const& m_flt) noexcept
{ // from GLM
    struct Double4x4 {
        double4 c[4];
        double4& operator[](size_t i)
        {
            return c[i];
        }
    };
    Double4x4 m;
    for (auto i : vstd::range(4))
    {
        m.c[i] = make_double4(m_flt.cols[i]);
    }
    const auto coef00 = m[2].z * m[3].w - m[3].z * m[2].w;
    const auto coef02 = m[1].z * m[3].w - m[3].z * m[1].w;
    const auto coef03 = m[1].z * m[2].w - m[2].z * m[1].w;
    const auto coef04 = m[2].y * m[3].w - m[3].y * m[2].w;
    const auto coef06 = m[1].y * m[3].w - m[3].y * m[1].w;
    const auto coef07 = m[1].y * m[2].w - m[2].y * m[1].w;
    const auto coef08 = m[2].y * m[3].z - m[3].y * m[2].z;
    const auto coef10 = m[1].y * m[3].z - m[3].y * m[1].z;
    const auto coef11 = m[1].y * m[2].z - m[2].y * m[1].z;
    const auto coef12 = m[2].x * m[3].w - m[3].x * m[2].w;
    const auto coef14 = m[1].x * m[3].w - m[3].x * m[1].w;
    const auto coef15 = m[1].x * m[2].w - m[2].x * m[1].w;
    const auto coef16 = m[2].x * m[3].z - m[3].x * m[2].z;
    const auto coef18 = m[1].x * m[3].z - m[3].x * m[1].z;
    const auto coef19 = m[1].x * m[2].z - m[2].x * m[1].z;
    const auto coef20 = m[2].x * m[3].y - m[3].x * m[2].y;
    const auto coef22 = m[1].x * m[3].y - m[3].x * m[1].y;
    const auto coef23 = m[1].x * m[2].y - m[2].x * m[1].y;
    const auto fac0 = double4{ coef00, coef00, coef02, coef03 };
    const auto fac1 = double4{ coef04, coef04, coef06, coef07 };
    const auto fac2 = double4{ coef08, coef08, coef10, coef11 };
    const auto fac3 = double4{ coef12, coef12, coef14, coef15 };
    const auto fac4 = double4{ coef16, coef16, coef18, coef19 };
    const auto fac5 = double4{ coef20, coef20, coef22, coef23 };
    const auto Vec0 = double4{ m[1].x, m[0].x, m[0].x, m[0].x };
    const auto Vec1 = double4{ m[1].y, m[0].y, m[0].y, m[0].y };
    const auto Vec2 = double4{ m[1].z, m[0].z, m[0].z, m[0].z };
    const auto Vec3 = double4{ m[1].w, m[0].w, m[0].w, m[0].w };
    const auto inv0 = Vec1 * fac0 - Vec2 * fac1 + Vec3 * fac2;
    const auto inv1 = Vec0 * fac0 - Vec2 * fac3 + Vec3 * fac4;
    const auto inv2 = Vec0 * fac1 - Vec1 * fac3 + Vec3 * fac5;
    const auto inv3 = Vec0 * fac2 - Vec1 * fac4 + Vec2 * fac5;
    constexpr auto sign_a = double4{ +1.0, -1.0, +1.0, -1.0 };
    constexpr auto sign_b = double4{ -1.0, +1.0, -1.0, +1.0 };
    const auto inv_0 = inv0 * sign_a;
    const auto inv_1 = inv1 * sign_b;
    const auto inv_2 = inv2 * sign_a;
    const auto inv_3 = inv3 * sign_b;
    const auto dot0 = m[0] * double4{ inv_0.x, inv_1.x, inv_2.x, inv_3.x };
    const auto dot1 = dot0.x + dot0.y + dot0.z + dot0.w;
    const auto one_over_determinant = 1.0 / dot1;
    return float4x4{ make_float4(inv_0 * one_over_determinant),
                     make_float4(inv_1 * one_over_determinant),
                     make_float4(inv_2 * one_over_determinant),
                     make_float4(inv_3 * one_over_determinant) };
}
} // namespace preparepass_detail
void PreparePass::on_enable(
    Pipeline const& pipeline,
    Device& device,
    CommandList& cmdlist,
    SceneManager& scene
)
{
    constexpr const float wavelength_min = 360;
    constexpr const float wavelength_max = 830;
    sobol_256d = heitz_sobol_256d(device, cmdlist);
    sobol_scrambling = heitz_sobol_scrambling(device, cmdlist, HeitzSobolSPP::SPP1);
    scene.bindless_allocator().set_reserved_buffer(heap_indices::sobol_256d_heap_idx, sobol_256d);
    scene.bindless_allocator().set_reserved_buffer(heap_indices::sobol_scrambling_heap_idx, sobol_scrambling);
    init_evt = luisa::fiber::async([&]() {
        ShaderManager::instance()->load("path_tracer/host_raytracer.bin", _host_raytracer);
    });
    static constexpr auto lut3d_size = spectrum::spectrum_lut3d_res * spectrum::spectrum_lut3d_res * spectrum::spectrum_lut3d_res * 3ull * sizeof(float4);
    static const uint3 transmission_ggx_energy_size{ 32u };
    static const size_t transmission_ggx_energy_size_bytes = transmission_ggx_energy_size.x * transmission_ggx_energy_size.y * transmission_ggx_energy_size.z * sizeof(float4);
    {

        luisa::vector<std::byte> vec;
        vec.resize_uninitialized(lut3d_size);
        _lut_load_cmds.emplace_back(
            luisa::fiber::async([&device, this, ptr = vec.data()]() {
                BinaryFileStream file_stream{ luisa::to_string(luisa::filesystem::path{luisa::current_executable_path()} / "rec2020.coeff") };
                LUISA_ASSERT(file_stream.length() == lut3d_size);
                file_stream.read({ ptr, lut3d_size });
                spectrum_lut_3d = device.create_volume<float>(PixelStorage::FLOAT4, uint3(spectrum::spectrum_lut3d_res * 3, spectrum::spectrum_lut3d_res, spectrum::spectrum_lut3d_res));
            }),
            std::move(vec),
            &spectrum_lut_3d
        );
    }
    {
        luisa::vector<std::byte> vec;
        vec.resize_uninitialized(transmission_ggx_energy_size_bytes);
        _lut_load_cmds.emplace_back(
            luisa::fiber::async([&device, this, ptr = vec.data()]() {
                BinaryFileStream file_stream{ luisa::to_string(luisa::filesystem::path{luisa::current_executable_path()} / "trans_ggx.bytes") };
                LUISA_ASSERT(file_stream.length() == transmission_ggx_energy_size_bytes);
                file_stream.read({ ptr, transmission_ggx_energy_size_bytes });
                transmission_ggx_energy = device.create_volume<float>(PixelStorage::FLOAT4, transmission_ggx_energy_size);
            }),
            std::move(vec),
            &transmission_ggx_energy
        );
    }

    // {
    //     BinaryFileStream file_stream(luisa::to_string(rbc::get_binary_path() / "Rec2020ToFourierEvenPacked.dat"));
    //     struct {
    //         size_t version, x, y, z;
    //     } header;
    //     file_stream.read({ reinterpret_cast<std::byte*>(&header), sizeof(header) });
    //     assert(header.version == 0);
    //     auto lut_resolution = make_uint3(header.x, header.y, header.z);
    //     auto buffer_size = sizeof(uint) * lut_resolution.x * lut_resolution.y * lut_resolution.z;
    //     auto lut_data = std::make_unique_for_overwrite<char[]>(buffer_size);
    //     file_stream.read({ reinterpret_cast<std::byte*>(lut_data.get()), buffer_size });
    //     srgb_to_fourier_even = device.create_volume<float>(PixelStorage::R10G10B10A2, lut_resolution);
    //     cmdlist << srgb_to_fourier_even.copy_from(lut_data.get());
    //     scene.dispose_after_commit(std::move(lut_data));
    //     LUISA_ASSERT(all(spectrum::srgb_to_fourier_even_size == lut_resolution));
    // }
    luisa::vector<float4> cie_xyz_lut_data;
    luisa::vector<float> illum_d65_lut_data;
    auto lut_counter = luisa::fiber::async([&]() {
        luisa::vector<float> ps;
        ps.reserve(spectrum::cie_xyz_cdfinv_size);
        for (size_t i = 0; i < spectrum::cie_xyz_cdfinv_size; i++)
        {
            ps.emplace_back(float(i) / (spectrum::cie_xyz_cdfinv_size - 1));
        }
        cie_xyz_lut_data.push_back_uninitialized(spectrum::cie_xyz_cdfinv_size);
        luisa::vector<float> xs, ys;
        for (size_t c = 0; c < 3; c++)
        {
            xs.clear();
            ys.clear();
            xs.reserve(size_t(wavelength_max - wavelength_min));
            ys.reserve(size_t(wavelength_max - wavelength_min));
            for (size_t i = 0; i <= size_t(wavelength_max - wavelength_min); i++)
            {
                xs.push_back(test::spectrum::CIE_xyz_1931_2deg[i].first);
                ys.push_back(test::spectrum::CIE_xyz_1931_2deg[i].second[c]);
            }
            auto result = preparepass_detail::generate_quantiles<float>(xs, ys, ps, 50, 10e-12f);
            for (size_t i = 0; i < spectrum::cie_xyz_cdfinv_size; i++)
            {
                cie_xyz_lut_data[i][c] = result[i];
            }
        }
    });
    {
        uint step = 10;
        uint lut_resolution = uint(wavelength_max - wavelength_min) / step + 1;
        illum_d65_lut_data.push_back_uninitialized(lut_resolution);
        auto& start = test::spectrum::CIE_std_illum_D65[0];
        for (size_t i = 0; i < lut_resolution; i++)
        {
            illum_d65_lut_data[i] = (1.0f / 98.8900106203f) * test::spectrum::CIE_std_illum_D65[size_t(wavelength_min) - start.first + i * step].second;
        }
        LUISA_ASSERT(spectrum::illum_d65_size == lut_resolution);
    }
    lut_counter.wait();
    cie_xyz_cdfinv = device.create_image<float>(PixelStorage::FLOAT4, make_uint2(spectrum::cie_xyz_cdfinv_size, 1));
    cmdlist << cie_xyz_cdfinv.copy_from(cie_xyz_lut_data.data());
    scene.dispose_after_commit(std::move(cie_xyz_lut_data));
    illum_d65 = device.create_image<float>(PixelStorage::FLOAT1, make_uint2(spectrum::illum_d65_size, 1));
    cmdlist << illum_d65.copy_from(illum_d65_lut_data.data());
    scene.dispose_after_commit(std::move(illum_d65_lut_data));
    // {
    //     uint step = 5;
    //     uint lut_resolution = uint(wavelength_max - wavelength_min) / step + 1;
    //     luisa::vector<half> lut_data(lut_resolution);
    //     for (size_t i = 0; i < lut_resolution; i++)
    //     {
    //         lut_data[i] = test::spectrum::BMESE_wavelength_to_phase[i].second;
    //     }
    //     bmese_phase = device.create_image<float>(PixelStorage::HALF1, make_uint2(lut_resolution, 1));
    //     cmdlist << bmese_phase.copy_from(lut_data.data());
    //     scene.dispose_after_commit(std::move(lut_data));
    //     LUISA_ASSERT(spectrum::bmese_phase_size == lut_resolution);
    // }
}

void PreparePass::wait_enable()
{
    init_evt.wait();
}

void PreparePass::early_update(Pipeline const& pipeline, PipelineContext const& ctx)
{
    init_evt.wait();
    for (auto& i : _lut_load_cmds)
    {
        i.evt.wait();
        (*ctx.cmdlist) << i.tex->copy_from(i.data.data());
        ctx.scene->dispose_after_commit(std::move(i.data));
    }
    _lut_load_cmds.clear();
    auto pass_ctx = ctx.mut.get_pass_context<PreparePassContext>(ctx.cam);
    auto frane_settings = ctx.mut.states.read_safe<FrameSettings>();
    // pass_ctx->last_cam.position += make_double3(frane_settings.global_offset);

    auto& mut = ctx.mut;
    auto jitter_data = mut.states.read<JitterData>();
    auto cam_data = mut.states.read<CameraData>();
    auto save_cam_data = vstd::scope_exit([&]() mutable {
        mut.states.write(std::move(cam_data));
        mut.states.write(std::move(jitter_data));
        mut.states.write(std::move(frane_settings));
    });
    jitter_data.last_jitter = jitter_data.jitter;
    cam_data.last_proj = cam_data.proj;
    cam_data.last_view = preparepass_detail::inverse_double(pass_ctx->last_cam.local_to_world_matrix());
    cam_data.last_sky_view = preparepass_detail::inverse_double(pass_ctx->last_cam.rotation_matrix());
    cam_data.last_vp = cam_data.last_proj * cam_data.last_view;
    cam_data.last_sky_vp = cam_data.last_proj * cam_data.last_sky_view;
    cam_data.last_inv_vp = preparepass_detail::inverse_double(cam_data.last_vp);
    switch (frane_settings.resource_color_space)
    {
    case ResourceColorSpace::Rec709:
        frane_settings.to_rec2020_matrix = make_float3x3(0.627404, 0.329283, 0.043313, 0.069097, 0.919540, 0.011362, 0.016391, 0.088013, 0.895595);
        break;
    case ResourceColorSpace::AdobeRGB:
        frane_settings.to_rec2020_matrix = make_float3x3(0.877334, 0.077494, 0.045172, 0.096623, 0.891527, 0.011850, 0.022921, 0.043037, 0.934042);
        break;
    case ResourceColorSpace::P3_D60:
        frane_settings.to_rec2020_matrix = make_float3x3(0.763906, 0.193206, 0.042888, 0.046355, 0.916212, 0.011251, -0.001227, 0.017124, 0.886810);
        break;
    case ResourceColorSpace::P3_D65:
        frane_settings.to_rec2020_matrix = make_float3x3(0.753833, 0.198597, 0.047570, 0.045744, 0.941777, 0.012479, -0.001210, 0.017602, 0.983609);
        break;
    case ResourceColorSpace::Rec2020:
        frane_settings.to_rec2020_matrix = float3x3::eye(1);
        break;
    }
    frane_settings.to_rec2020_matrix = transpose(frane_settings.to_rec2020_matrix);

    auto& scene = *ctx.scene;
    auto&& alloc = scene.bindless_allocator();
    auto emplace_buffer = [&](uint idx, auto&& buffer) {
        if (buffer)
        {
            scene.bindless_allocator().set_reserved_buffer(idx, buffer);
        }
        else
        {
            scene.bindless_allocator().remove_reserved_buffer(idx);
        }
    };
    auto emplace_tex2d = [&](uint idx, auto&& image, Sampler sampler) {
        if (image)
        {
            scene.bindless_allocator().set_reserved_tex2d(idx, image, sampler);
        }
        else
        {
            scene.bindless_allocator().remove_reserved_tex2d(idx);
        }
    };
    auto emplace_tex3d = [&](uint idx, auto&& volume, Sampler sampler) {
        if (volume)
        {
            scene.bindless_allocator().set_reserved_tex3d(idx, volume, sampler);
        }
        else
        {
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
    emplace_tex2d(heap_indices::cie_xyz_cdfinv_idx, cie_xyz_cdfinv, Sampler::linear_point_mirror());

    jitter_data.jitter = float2(0.f);
    cam_data.inv_view = ctx.cam.local_to_world_matrix();
    cam_data.inv_sky_view = ctx.cam.rotation_matrix();
    cam_data.view = preparepass_detail::inverse_double(cam_data.inv_view);
    cam_data.proj = ctx.projection;
    cam_data.vp = cam_data.proj * cam_data.view;
    cam_data.inv_proj = preparepass_detail::inverse_double(cam_data.proj);
    cam_data.inv_vp = preparepass_detail::inverse_double(cam_data.vp);
    if (frane_settings.frame_index != 0)
    {
        pass_ctx->last_cam = ctx.cam;
    }
    else
    {
        cam_data.last_proj = cam_data.proj;
        jitter_data.last_jitter = jitter_data.jitter;
    }
}
void PreparePass::update(Pipeline const& pipeline, PipelineContext const& ctx)
{
    auto& reqs = ctx.mut.click_manager._requires;
    if (!reqs.empty())
    {
        size_t size = 16384;
        while (size < reqs.size())
        {
            size *= 2;
        }
        auto& render_device = RenderDevice::instance();
        Buffer<RayInput> ray_input_buffer = render_device.create_transient_buffer<RayInput>("ray_input", size);
        Buffer<RayOutput> ray_output_buffer = render_device.create_transient_buffer<RayOutput>("ray_output", size);
        if (ctx.scene->accel() && ctx.scene->accel().size() > 0 && reqs.size() > 0)
        {
            luisa::vector<RayInput> ray_inputs;
            luisa::vector<RayOutput> ray_outputs;
            ray_inputs.push_back_uninitialized(reqs.size());
            ray_outputs.push_back_uninitialized(reqs.size());
            auto const& cam_data = ctx.mut.states.read<CameraData>();
            for (auto i : vstd::range(ray_inputs.size()))
            {
                auto& inp = reqs[i].second;
                luisa::visit(
                    [&]<typename T>(T const& t) {
                        if constexpr (std::is_same_v<T, RayCastRequire>)
                        {
                            ray_inputs[i] = RayInput{
                                .ray_origin = { t.ray_origin.x, t.ray_origin.y, t.ray_origin.z },
                                .ray_dir = { t.ray_dir.x, t.ray_dir.y, t.ray_dir.z },
                                .t_min = t.t_min,
                                .t_max = t.t_max,
                                .mask = static_cast<uint>(t.mask)
                            };
                        }
                        else
                        {
                            auto ray_origin = cam_data.inv_vp * make_float4(t.screen_uv * 2.0f - 1.0f, 1.0f, 1.0f);
                            auto ray_dst = cam_data.inv_vp * make_float4(t.screen_uv * 2.0f - 1.0f, 0.0f, 1.0f);
                            ray_origin /= ray_origin.w;
                            ray_dst /= ray_dst.w;
                            auto ray_t = distance(ray_dst.xyz(), ray_origin.xyz());
                            auto ray_dir = (ray_dst.xyz() - ray_origin.xyz()) / max(1e-4f, ray_t);
                            ray_inputs[i] = RayInput{
                                .ray_origin = { ray_origin.x, ray_origin.y, ray_origin.z },
                                .ray_dir = { ray_dir.x, ray_dir.y, ray_dir.z },
                                .t_min = 0.f,
                                .t_max = ray_t,
                                .mask = static_cast<uint>(t.mask)
                            };
                        }
                    },
                    inp
                );
            }
            auto input_buffer_view = ray_input_buffer.view(0, reqs.size());
            auto output_buffer_view = ray_output_buffer.view(0, reqs.size());
            (*ctx.cmdlist)
                << input_buffer_view.copy_from(ray_inputs.data())
                << (*_host_raytracer)(
                       ctx.scene->accel(),
                       ctx.scene->buffer_heap(),
                       input_buffer_view,
                       output_buffer_view,
                       heap_indices::inst_buffer_heap_idx,
                       heap_indices::mat_idx_buffer_heap_idx
                   )
                       .dispatch(reqs.size())
                << output_buffer_view.copy_to(ray_outputs.data());
            ctx.cmdlist->add_callback(
                [click_mng = &ctx.mut.click_manager,
                 _requires = std::move(reqs),
                 ray_outputs = std::move(ray_outputs)]() {
                    for (auto i : vstd::range(ray_outputs.size()))
                    {
                        auto& outputs = ray_outputs[i];
                        RayCastResult result{
                            .mat_code = MatCode{ outputs.mat_code },
                            .inst_id = outputs.tlas_inst_id,
                            .prim_id = outputs.blas_prim_id,
                            .submesh_id = outputs.blas_submesh_id,
                            .ray_length = outputs.ray_t,
                            .triangle_bary = float2(outputs.triangle_bary[0], outputs.triangle_bary[1])
                        };
                        std::lock_guard lck{ click_mng->_mtx };
                        click_mng->_results.try_emplace(
                            _requires[i].first,
                            result
                        );
                    }
                }
            );
            ctx.scene->dispose_after_commit(std::move(ray_inputs));
        }
    }
}
void PreparePass::on_frame_end(
    Pipeline const& pipeline,
    Device& device,
    SceneManager& scene
)
{
    auto&& alloc = scene.bindless_allocator();
}
void PreparePass::on_disable(
    Pipeline const& pipeline,
    Device& device,
    CommandList& cmdlist,
    SceneManager& scene
)
{
    auto&& alloc = scene.bindless_allocator();
}
PreparePass::~PreparePass()
{
    for (auto& i : _lut_load_cmds)
    {
        i.evt.wait();
    }
    _lut_load_cmds.clear();
}
} // namespace rbc