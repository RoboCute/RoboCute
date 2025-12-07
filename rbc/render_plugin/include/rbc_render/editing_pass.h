#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/raster/depth_buffer.h>
#include <luisa/core/fiber.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/click_manager.h>
#include <raster/raster_args.hpp>
#include <rbc_graphics/scene_manager.h>

namespace rbc {
struct EditingPass : Pass {
private:
    luisa::fiber::counter _init_counter;
    RasterShader<Buffer<geometry::RasterElement>, float4x4> const *_contour_draw;
    Shader2D<Image<float>, Image<float>, int2, int, float> const *_contour_flood;
    Shader2D<Image<float>, Image<float>, Image<float>, float3> const *_contour_reduce;
    Shader2D<Image<float>, float4, int2> const *_draw_frame_selection;
    ShaderBase const *_click_pick;

public:
    void on_enable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    void update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void on_disable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    EditingPass();
    ~EditingPass();
    Image<float> emission;
    void wait_enable() override;
    void contour(PipelineContext const &ctx, luisa::span<uint const> draw_indices);
};
}// namespace rbc

RBC_RTTI(rbc::EditingPass)