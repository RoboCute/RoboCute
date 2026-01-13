#pragma once
#include <rbc_plugin/plugin.h>
#include <luisa/core/mathematics.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/rhi/pixel.h>
#include <luisa/vstl/functional.h>
#include <rbc_core/rc.h>
namespace rbc {
struct StateMap;
struct DeviceImage;
struct DenoisePack {
    // not really buffer
    luisa::compute::Buffer<float> external_albedo;
    luisa::compute::Buffer<float> external_normal;
    luisa::compute::Buffer<float> external_input;
    luisa::compute::Buffer<float> external_output;
    vstd::function<void()> denoise_callback;
};
struct RenderPlugin : Plugin {
    struct PipeCtxStub {};
    virtual PipeCtxStub *create_pipeline_context() = 0;
    virtual StateMap *pipe_ctx_state_map(PipeCtxStub *ctx) = 0;
    virtual void destroy_pipeline_context(PipeCtxStub *ctx) = 0;
    virtual void sync_init() = 0;

    // render_loop
    virtual bool initialize_pipeline(luisa::string_view pipeline_name) = 0;
    virtual void clear_context(PipeCtxStub *ctx) = 0;
    virtual bool before_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
    virtual bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
    // skybox
    virtual void update_skybox(RC<DeviceImage> image) = 0;
    virtual bool update_skybox(
        luisa::filesystem::path const &path,
        luisa::compute::PixelStorage pixel_storage,
        luisa::uint2 resolution,
        uint64_t file_offset_bytes = 0) = 0;
    virtual void dispose_skybox() = 0;
    // denoising
    virtual bool init_oidn() = 0;
    virtual DenoisePack create_denoise_task(
        luisa::compute::Stream &stream,
        PipeCtxStub *ctx,
        luisa::uint2 render_resolution) = 0;
    virtual void destroy_denoise_task(luisa::compute::Stream &stream) = 0;
protected:
    ~RenderPlugin() = default;
};
}// namespace rbc