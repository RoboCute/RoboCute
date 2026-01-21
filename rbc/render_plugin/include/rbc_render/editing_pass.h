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
    struct PixelArgs {
        uint2 clicked_pixel;
        float3 from_mapped_color;
        float3 to_mapped_color;
    };
    MeshFormat _gizmos_mesh_format{};
    luisa::fiber::counter _init_counter;
    RasterShader<Buffer<geometry::RasterElement>, float4x4> const *_contour_draw;
    RasterShader<float4x4, Buffer<float4>, PixelArgs> const *_draw_gizmos;
    RasterShader<float4x4,
                 float3,//cam_pos,
                 float3,//color,
                 float, //start_decay_dist,
                 float  //decay_length
                 > const *_draw_grid;
    Shader1D<Buffer<uint>, uint> const *_clear_buffer;
    Shader2D<Image<float>, Image<float>, int2, int, float> const *_contour_flood;
    Shader2D<Image<float>, Image<float>, Image<float>, float3> const *_contour_reduce;
    Shader2D<Image<float>, float4, int2> const *_draw_frame_selection;
    Shader1D<
        Buffer<float4>,// &result_poses,
        float3,        //grid_tangent,
        float3,        //grid_bitangent,
        float3,        //center,
        float2,        //interval_size,
        uint2          //grid_size
        > const *_grid_gen;
    ShaderBase const *_click_pick;
    Buffer<float4> _draw_grid_buffer;

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
    void wait_enable() override;
    void contour(PipelineContext const &ctx, luisa::span<uint const> draw_indices);
};
}// namespace rbc

RBC_RTTI(rbc::EditingPass)