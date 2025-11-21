#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/core/fiber.h>
#include <rbc_render/pipeline_context.h>
namespace rbc
{
#define OFFLINE_MODE
#include <path_tracer/pt_args.hpp>
#include <sampling/denoise_training_data.hpp>
#undef OFFLINE_MODE
namespace pt
{
struct GBuffer {
    std::array<float, 4> hitpos_normal;
    std::array<float, 3> beta;
    std::array<float, 3> radiance;
};
} // namespace pt
struct PTPassContext;
struct  OfflinePTPass : public Pass {
private:
    friend struct PTPassContext;
    luisa::fiber::counter init_counter;

    using ClearHashGrid =
        Shader1D<
            Buffer<uint>, //& key_buffer,
            Buffer<uint>, //& value_buffer
            uint          // max_accum
            >;
    using AccumHashGrid =
        Shader2D<
            Buffer<pt::GBuffer>, //& gbuffers,
            Buffer<uint>,        //& key_buffer,
            Buffer<uint>,        //& value_buffer,
            Image<uint>,         //& surfel_mark,
            float2,              // jitter,
            float3,              // cam_pos,
            float,               // grid_size,
            uint,                // buffer_size,
            uint,                // max_offset,
            uint                 // max_accum
            >;
    using IntegrateHashGrid = Shader2D<
#ifdef OUTPUT_DENOISE_TRAINING_DATA
        Buffer<DenoiseTrainingData>,
#endif
        Buffer<pt::GBuffer>, //& gbuffers,
        Image<float>,        //& emission_img,
        Buffer<uint>,        //& value_buffer,
        Image<uint>,         //& surfel_mark,
        uint,                // max_accum
        float                // rate
        >;
    ShaderBase const* pt_shader;
    Shader1D<Buffer<uint>, uint> const* clear_ptr_buffer;
    ShaderBase const* pt_shader_denoise;
    ShaderBase const* multi_bounce;
    ClearHashGrid const* clear_hashgrid;
    AccumHashGrid const* accum_hashgrid;
    IntegrateHashGrid const* integrate_hashgrid;

public:
    Buffer<uint> key_buffer;
    Buffer<uint> value_buffer;

    uint sky_heap_idx = ~0u;
    uint sky_lum_idx = ~0u;
    uint alias_heap_idx = ~0u;
    uint pdf_heap_idx = ~0u;

    void on_enable(
        Pipeline const& pipeline,
        Device& device,
        CommandList& cmdlist,
        SceneManager& scene
    ) override;
    void update(Pipeline const& pipeline, PipelineContext const& ctx) override;
    void early_update(Pipeline const& pipeline, PipelineContext const& ctx) override;
    void on_frame_end(
        Pipeline const& pipeline,
        Device& device,
        SceneManager& scene
    ) override;
    void on_disable(
        Pipeline const& pipeline,
        Device& device,
        CommandList& cmdlist,
        SceneManager& scene
    ) override;
    void wait_enable() override;
    ~OfflinePTPass();
};
struct  PTPassContext : public PassContext {
public:
#ifdef OUTPUT_DENOISE_TRAINING_DATA
    Buffer<OfflinePTPass::DenoiseTrainingData> denoise_buffer;
    luisa::vector<OfflinePTPass::DenoiseTrainingData> train_data;
#endif
    Buffer<float> albedo_buffer;
    Buffer<float> normal_buffer;
    uint gbuffer_accumed_frame{};
    bool noisy_initialized{ false };
    PTPassContext();
    ~PTPassContext();
};
} // namespace rbc

RBC_RTTI(rbc::PTPassContext)
RBC_RTTI(rbc::OfflinePTPass)