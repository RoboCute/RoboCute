#include <rbc_render/raster_pass.h>
#include <rbc_graphics/frustum.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>
#include <luisa/backends/ext/raster_ext.hpp>
namespace rbc {
namespace draw_raster {
#include <raster/draw_raster.inl>
}// namespace draw_raster
void RasterPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
#define RBC_LOAD_SHADER(SHADER_NAME, NAME_SPACE, PATH) \
    _init_counter.add();                               \
    luisa::fiber::schedule([this]() {                  \
        SHADER_NAME = NAME_SPACE::load_shader(PATH);   \
        _init_counter.done();                          \
    })

    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/accel_id.bin", _draw_id_shader);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/clear_id.bin",
        _clear_id);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/shading_id.bin",
        _shading_id);
    RBC_LOAD_SHADER(_shading, draw_raster, "raster/draw_raster.bin");
}
#undef RBC_LOAD_SHADER
void RasterPass::wait_enable() {
    _init_counter.wait();
}
void RasterPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto &sm = SceneManager::instance();
    ctx.scene->accel_manager().dispose_accel(*ctx.cmdlist, sm.dispose_queue());
}
void RasterPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    const auto &cam = ctx.pipeline_settings.read<Camera>();
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    const auto &cam_data = ctx.pipeline_settings.read<CameraData>();
    if (pass_ctx->depth_buffer && any(pass_ctx->depth_buffer.size() != frame_settings.render_resolution)) {
        sm.dispose_after_sync(std::move(pass_ctx->depth_buffer));
    }
    if (!pass_ctx->depth_buffer) {
        pass_ctx->depth_buffer = render_device.lc_device().create_depth_buffer(DepthFormat::D32, frame_settings.render_resolution);
    }
    AccelManager::DrawListMap draw_meshes;
    BufferView<AccelManager::RasterElement> data_buffer;
    auto &cmdlist = *ctx.cmdlist;
    auto frustum_corners = cam.frustum_corners();
    auto frustum_planes = cam.frustum_plane();
    auto frustum_min_point = frustum_corners[0];
    auto frustum_max_point = frustum_corners[0];
    for (auto i : vstd::range(1, 8)) {
        frustum_min_point = min(frustum_min_point, frustum_corners[i]);
        frustum_max_point = max(frustum_max_point, frustum_corners[i]);
    }
    static bool bb = false;
    auto frustum_cull_callback = [&](float4x4 const &transform, AABB const &bounding) {
        return frustum_cull(make_double4x4(transform), bounding, frustum_planes, frustum_min_point, frustum_max_point, cam.dir_forward(), cam.position);
    };
    sm.accel_manager().make_draw_list(
        cmdlist,
        sm.after_commit_dsp_queue(),
        sm.dispose_queue(),
        std::move(frustum_cull_callback),
        draw_meshes, data_buffer);
    RasterState raster_state{
        .cull_mode = CullMode::None,
        .depth_state = DepthState{
            .enable_depth = true,
            .comparison = Comparison::Greater,
            .write = true},
    };
    auto raster_ext = render_device.lc_device().extension<RasterExt>();
    Image<float> emission = render_device.create_transient_image<float>("emission", PixelStorage::BYTE4, frame_settings.render_resolution, 1, false, true);
    auto id_map = render_device.create_transient_image<uint>("id_map", PixelStorage::INT4, frame_settings.render_resolution, 1, false, true);
    cmdlist << pass_ctx->depth_buffer.clear(0.0f)
            << (*_clear_id)(id_map, uint4(-1, -1, 0, 0)).dispatch(frame_settings.render_resolution);
    //
    if (!draw_meshes.empty()) {
        raster::VertArgs vert_args{
            .view = cam_data.view,
            .proj = cam_data.proj,
            .view_proj = cam_data.vp};
        cmdlist << (*_draw_id_shader)(data_buffer, vert_args)
                       .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, id_map);
    }
    // cmdlist << (*_shading_id)(id_map, emission).dispatch(frame_settings.render_resolution);
    const auto &sky_heap = ctx.pipeline_settings.read<SkyHeapIndices>();
    float3 light_dir = make_float3(cam.dir_forward());
    float3 light_color{1};
    cmdlist << draw_raster::dispatch_shader(_shading, frame_settings.render_resolution, sm.tex_streamer().level_buffer(), sm.buffer_heap(), sm.image_heap(), id_map, emission, sm.accel_manager().last_trans_buffer(), cam_data.inv_vp, frame_settings.to_rec2020_matrix, cam_data.world_to_sky, make_float3(cam.position), sky_heap.sky_heap_idx, sm.tex_streamer().countdown(), light_dir, light_color);
    frame_settings.resolved_img = std::move(emission);
}
void RasterPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {}
RasterPass::RasterPass() {}
RasterPass::~RasterPass() {}
RasterPassContext::RasterPassContext() {}
RasterPassContext::~RasterPassContext() {}
}// namespace rbc
