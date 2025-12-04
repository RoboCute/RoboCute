#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/core/fiber.h>
#include <rbc_render/pipeline_context.h>
namespace rbc {
#define OFFLINE_MODE
#include <path_tracer/pt_args.hpp>
#undef OFFLINE_MODE
namespace pt {
struct GBuffer {
    std::array<float, 4> hitpos_normal;
    std::array<float, 3> beta;
    std::array<float, 3> radiance;
};
}// namespace pt
struct PTPassContext;
struct OfflinePTPass : public Pass {
private:
    friend struct PTPassContext;
    luisa::fiber::counter init_counter;

    using ClearHashGrid =
        Shader1D<
            Buffer<uint>,//& key_buffer,
            Buffer<uint>,//& value_buffer
            uint         // max_accum
            >;
    using AccumHashGrid =
        Shader2D<
            Buffer<pt::GBuffer>,//& gbuffers,
            Buffer<uint>,       //& key_buffer,
            Buffer<uint>,       //& value_buffer,
            Image<uint>,        //& surfel_mark,
            float2,             // jitter,
            float3,             // cam_pos,
            float,              // grid_size,
            uint,               // buffer_size,
            uint,               // max_offset,
            uint                // max_accum
            >;
    using IntegrateHashGrid = Shader2D<
        Buffer<pt::GBuffer>,//& gbuffers,
        Image<float>,       //& emission_img,
        Buffer<uint>,       //& value_buffer,
        Image<uint>,        //& surfel_mark,
        uint,               // max_accum
        float               // rate
        >;
    using DrawSkyShader = Shader2D<
        Image<float>, // _emission,
        BindlessArray,//& image_heap,
        BindlessArray,//& volume_heap,
        uint,         //sky_idx,
        float3x3,     //resource_to_rec2020_mat,
        float3x3,     //world_2_sky_mat,
        float4x4,     //inv_vp,
        float3,       //cam_pos,
        float2,       //jitter
        uint          // frame_index
        >;
    DrawSkyShader const *draw_sky_shader;
    ShaderBase const *pt_shader;
    Shader1D<Buffer<uint>, uint> const *clear_ptr_buffer;
    ShaderBase const *pt_shader_denoise;
    ShaderBase const *multi_bounce;
    ClearHashGrid const *clear_hashgrid;
    AccumHashGrid const *accum_hashgrid;
    IntegrateHashGrid const *integrate_hashgrid;

public:
    Buffer<uint> key_buffer;
    Buffer<uint> value_buffer;

    void on_enable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    void update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void early_update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void on_frame_end(
        Pipeline const &pipeline,
        Device &device,
        SceneManager &scene) override;
    void on_disable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    void wait_enable() override;
    ~OfflinePTPass();
};
struct PTPassContext : public PassContext {
public:
    uint gbuffer_accumed_frame{};
    PTPassContext();
    ~PTPassContext();
};
}// namespace rbc

RBC_RTTI(rbc::PTPassContext)
RBC_RTTI(rbc::OfflinePTPass)