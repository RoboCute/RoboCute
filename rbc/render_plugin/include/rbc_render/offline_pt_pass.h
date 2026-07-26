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
enum struct AlphaCull : uint32_t;
struct FrameSettings;
struct RenderDevice;
struct AccumPassContext;
struct CameraData;
struct Camera;
struct SkyHeapIndices;
struct JitterData;
struct PathTracerSettings;
struct PreparePass;

struct OfflinePTPass : public Pass {
private:
    friend struct PTPassContext;
    luisa::fiber::counter _init_counter;

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
        Image<uint>,  // _emission,
        BindlessArray,//& image_heap,
        BindlessArray,//& volume_heap,
        uint,         //sky_idx,
        float3x3,     //resource_to_rec2020_mat,
        SpectrumAccumulationArgs,//spectrum accumulation,
        float3x3,     //world_2_sky_mat,
        float4x4,     //inv_vp,
        float3,       //cam_pos,
        float2,       //jitter
        uint,         // frame_index
        bool          //write_id_map
        >;
    struct PTShaders;
    vstd::unique_ptr<PTShaders> _pt;
    DrawSkyShader const *_draw_sky_shader{nullptr};
    Shader1D<Buffer<uint>, uint> const *_clear_ptr_buffer{nullptr};
    ShaderBase const *_ao_trace{nullptr};
    ClearHashGrid const *_clear_hashgrid{nullptr};
    AccumHashGrid const *_accum_hashgrid{nullptr};
    IntegrateHashGrid const *_integrate_hashgrid{nullptr};
    PreparePass *_prepare_pass{nullptr};

    struct PreparedResources {
        Image<float> emission;
        Image<uint> const *id_map{nullptr};
        Image<uint> id_map_val;
        Image<uint> surfel_mark;
        Buffer<offline::MultiBouncePixel> multibounce_buffer;
        Buffer<uint> multibounce_buffer_counter;
        Buffer<pt::GBuffer> geo_buffer;
    };

    struct PTResourceContext {
        Pipeline const &pipeline;
        PipelineContext const &ctx;
        SceneManager &scene;
        CommandList &cmdlist;
        FrameSettings &frame_settings;
        RenderDevice &render_device;
        AccumPassContext *accum_pass_ctx;
    };

    PreparedResources _prepare_resources(const PTResourceContext &rc) const;
    offline::PTArgs _setup_pt_args(
        const PTResourceContext &rc,
        const CameraData &cam_data,
        const Camera &cam,
        const SkyHeapIndices &sky_heap,
        bool write_id_map,
        uint32_t frame_index) const;
    void _draw_sky_only(
        const PTResourceContext &rc,
        const Image<float> &emission,
        Image<uint> const *id_map,
        const SkyHeapIndices &sky_heap,
        const CameraData &cam_data,
        const Camera &cam,
        const JitterData &jitter_data,
        bool write_id_map) const;
    void _trace_ao_sample(
        const PTResourceContext &rc,
        const Image<float> &emission,
        const offline::PTArgs &pt_args,
        const PathTracerSettings &pt_settings) const;
    void _dispatch_path_tracing(
        const PTResourceContext &rc,
        const PreparedResources &resources,
        offline::PTArgs &pt_args,
        Image<uint> const *id_map,
        uint32_t geometry_mask,
        AlphaCull alpha_cull) const;
    void _process_multibounce_indirect(
        const PTResourceContext &rc,
        const PreparedResources &resources,
        const offline::PTArgs &pt_args,
        float accumulate_rate) const;
    void _clear_multibounce_counter(const PTResourceContext &rc, const Buffer<uint> &counter) const;

public:
    Buffer<uint> key_buffer;
    Buffer<uint> value_buffer;

    OfflinePTPass();

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
    uint64_t shader_revision{};

    PTPassContext();
    ~PTPassContext();
};

}// namespace rbc

RBC_RTTI(rbc::PTPassContext)
RBC_RTTI(rbc::OfflinePTPass)
