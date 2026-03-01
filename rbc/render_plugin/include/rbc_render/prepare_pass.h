#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/shader.h>
#include <luisa/core/fiber.h>
#include <rbc_render/renderer_data.h>
namespace rbc {
struct PreparePass : public Pass {
    struct LutLoadCmd {
        luisa::fiber::event evt;
        luisa::vector<std::byte> data;
        Volume<float> const *tex;
    };
    luisa::vector<LutLoadCmd> _lut_load_cmds;

public:
    Buffer<uint> sobol_256d;
    Buffer<uint> sobol_scrambling;
    Buffer<uint> sobol_ranking;

    Volume<float> spectrum_lut_3d;
    // Volume<float> srgb_to_fourier_even;
    Image<float> cie_xyz_cdfinv;
    // Image<float> bmese_phase;
    Image<float> illum_d65;

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