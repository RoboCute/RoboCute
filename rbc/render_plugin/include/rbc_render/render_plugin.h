#pragma once
#include <luisa/vstl/common.h>
#include <rbc_graphics/camera.h>
#include <rbc_core/state_map.h>
#include <rbc_runtime/plugin.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/stream.h>
namespace rbc {
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
    virtual PipeCtxStub *create_pipeline_context(StateMap &render_settings_map) = 0;
    virtual void destroy_pipeline_context(PipeCtxStub *ctx) = 0;
    virtual Camera &get_camera(PipeCtxStub *pipe_ctx) = 0;

    // render_loop
    virtual bool initialize_pipeline(luisa::string_view pipeline_name) = 0;
    virtual void clear_context(PipeCtxStub *ctx) = 0;
    virtual bool before_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
    virtual bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
    // skybox
    virtual bool update_skybox(
        luisa::filesystem::path const &path,
        uint2 resolution,
        uint64_t file_offset_bytes = 0) = 0;
    virtual void dispose_skybox() = 0;
    // denoising
    virtual bool init_oidn() = 0;
    virtual DenoisePack create_denoise_task(
        luisa::compute::Stream &stream,
        uint2 render_resolution) = 0;
    virtual void destroy_denoise_task(luisa::compute::Stream &stream) = 0;
protected:
    ~RenderPlugin() = default;
};
}// namespace rbc