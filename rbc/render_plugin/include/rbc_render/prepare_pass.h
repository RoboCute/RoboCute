#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/shader.h>
#include <luisa/core/fiber.h>
#include <rbc_render/renderer_data.h>
#include <spectrum/spectrum_args.hpp>
#include <array>
namespace rbc {
struct PreparePassContext;
struct PreparePass : public Pass {
    struct LutLoadCmd {
        luisa::fiber::event evt;
        luisa::vector<std::byte> data;
        Volume<float> const *tex;
    };
    luisa::vector<LutLoadCmd> _lut_load_cmds;

    void _load_rec2020_lut(Device &device, luisa::filesystem::path const &runtime_dir);
    void _load_transmission_ggx_lut(Device &device, luisa::filesystem::path const &runtime_dir);
    static constexpr size_t spectrum_accumulation_space_count = 2u;
    std::array<luisa::vector<float4>, spectrum_accumulation_space_count> _spectrum_lut_data;
    std::array<SpectrumAccumulationArgs, spectrum_accumulation_space_count> _spectrum_args;
    uint _active_spectrum_accumulation_space{~0u};

    double _compute_spectrum_luts();
    luisa::vector<float> _compute_illum_d65_lut(double d65_normalization);
    void _initialize_sobol_resources(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene,
        luisa::filesystem::path const &runtime_dir);
    void _create_and_upload_images(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene,
        luisa::vector<float> &&illum_d65_lut_data);
    void _update_spectrum_accumulation_space(PipelineContext const &ctx);

    // Early update helper functions
    void _process_lut_load_commands(PipelineContext const &ctx);
    void _update_camera_aspect_ratio(PipelineContext const &ctx, Camera &cam);
    void _initialize_first_frame(PipelineContext const &ctx, PreparePassContext *pass_ctx, Camera &cam);
    void _update_last_frame_camera_data(PipelineContext const &ctx, PreparePassContext *pass_ctx);
    void _set_color_space_matrix(PipelineContext const &ctx);
    void _bind_resources_to_heap(SceneManager &scene);
    void _update_current_frame_camera_data(PipelineContext const &ctx, Camera &cam);
    void _update_pass_context(PipelineContext const &ctx, PreparePassContext *pass_ctx, Camera &cam, bool is_first_frame);

public:
    Buffer<uint> sobol_256d;
    Buffer<uint> sobol_scrambling;
    Buffer<uint> sobol_ranking;

    Volume<float> spectrum_lut_3d;
    // Volume<float> srgb_to_fourier_even;
    Image<float> spectrum_wavelength_lut;
    // Image<float> bmese_phase;
    Image<float> illum_d65;
    SpectrumAccumulationArgs spectrum_args;

    Volume<float> transmission_ggx_energy;

    struct RayInput {
        std::array<float, 3> ray_origin;
        std::array<float, 3> ray_dir;
        float t_min;
        float t_max;
        uint mask;
    };

    struct RayOutput {
        uint mat_code;
        float ray_t;
        uint tlas_inst_id;
        uint blas_prim_id;
        uint blas_submesh_id;
        std::array<float, 2> triangle_bary;
    };
    void on_enable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    void early_update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void on_frame_end(
        Pipeline const &pipeline,
        Device &device,
        SceneManager &scene) override;
    void on_disable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    ~PreparePass();
    void wait_enable() override;
};
}// namespace rbc
RBC_RTTI(rbc::PreparePass)
