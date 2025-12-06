#include <rbc_render/raster_pass.h>
#include <rbc_graphics/frustum.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>
#include <luisa/backends/ext/raster_ext.hpp>
namespace rbc {
namespace click_pick {
#include <raster/click_pick.inl>
}// namespace click_pick
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
    // _init_counter.add();
    // luisa::fiber::schedule([this] {
    //     _draw_scene_shader =
    //         ShaderManager::instance()
    //             ->load_raster_shader<Buffer<AccelManager::RasterElement>, raster::VertArgs>("raster/test_raster.bin");
    //     _init_counter.done();
    // });
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/accel_id.bin", _draw_id_shader);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/clear_id.bin",
        _clear_id);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/shading_id.bin",
        _shading_id);
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/contour_draw.bin", _contour_draw);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_flood.bin", _contour_flood);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_reduce.bin", _contour_reduce);
    RBC_LOAD_SHADER(_click_pick, click_pick, "raster/click_pick.bin");
}
#undef RBC_LOAD_SHADER
void RasterPass::wait_enable() {
    _init_counter.wait();
}

void RasterPass::contour(PipelineContext const &ctx, luisa::span<uint const> draw_indices) {
    if (draw_indices.empty()) return;
    RasterState raster_state{
        .cull_mode = CullMode::None,
    };
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    luisa::vector<RasterMesh> meshes;
    luisa::vector<geometry::RasterElement> host_data;
    meshes.reserve(draw_indices.size());
    host_data.reserve(draw_indices.size());
    auto elem_buffer = sm.host_upload_buffer().allocate_upload_buffer<geometry::RasterElement>(draw_indices.size());
    for (auto &i : draw_indices) {
        auto draw_cmd = sm.accel_manager().draw_object(i, 1, meshes.size());
        meshes.push_back(std::move(draw_cmd.mesh));
        host_data.push_back(draw_cmd.info);
    }
    std::memcpy(elem_buffer.mapped_ptr(), host_data.data(), host_data.size_bytes());
    const auto &cam_data = ctx.pipeline_settings->read<CameraData>();
    auto &frame_settings = ctx.pipeline_settings->read_mut<FrameSettings>();
    auto &cmdlist = *ctx.cmdlist;
    auto raster_ext = render_device.lc_device().extension<RasterExt>();
    auto origin_map = render_device.create_transient_image<float>(
        "contour_origin",
        PixelStorage::BYTE1,
        frame_settings.render_resolution,
        1, false, true);
    Image<float>
        contour_imgs[2]{
            render_device.create_transient_image<float>(
                "contour_temp0",
                PixelStorage::BYTE1,
                frame_settings.render_resolution),
            render_device.create_transient_image<float>(
                "contour_temp1",
                PixelStorage::BYTE1,
                frame_settings.render_resolution)};
    cmdlist << raster_ext->clear_render_target(
                   origin_map,
                   float4(0))
            << (*_contour_draw)(
                   elem_buffer.view,
                   cam_data.vp)
                   .draw(
                       std::move(meshes),
                       sm.accel_manager().basic_foramt(),
                       Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, nullptr, origin_map);
    auto const *src_img = &origin_map;
    for (int i = 0; i < 2; ++i) {
        cmdlist << (*_contour_flood)(
                       *src_img,
                       contour_imgs[1],
                       int2(0, 1),
                       3,
                       2.0f)
                       .dispatch(frame_settings.render_resolution)
                << (*_contour_flood)(
                       contour_imgs[1],
                       contour_imgs[0],
                       int2(1, 0),
                       3,
                       2.0f)
                       .dispatch(frame_settings.render_resolution);
        src_img = &contour_imgs[0];
    }
    cmdlist << (*_contour_reduce)(
                   origin_map,
                   *src_img,
                   emission,
                   float3(1.0f, 1.0f, 1.0f))
                   .dispatch(frame_settings.render_resolution);
}

void RasterPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &frame_settings = ctx.pipeline_settings->read_mut<FrameSettings>();
    const auto &cam_data = ctx.pipeline_settings->read<CameraData>();
    if (pass_ctx->depth_buffer && any(pass_ctx->depth_buffer.size() != frame_settings.render_resolution)) {
        sm.dispose_after_sync(std::move(pass_ctx->depth_buffer));
    }
    if (!pass_ctx->depth_buffer) {
        pass_ctx->depth_buffer = render_device.lc_device().create_depth_buffer(DepthFormat::D32, frame_settings.render_resolution);
    }
    AccelManager::DrawListMap draw_meshes;
    BufferView<AccelManager::RasterElement> data_buffer;
    auto &cmdlist = *ctx.cmdlist;
    auto frustum_corners = ctx.cam.frustum_corners();
    auto frustum_planes = ctx.cam.frustum_plane();
    auto frustum_min_point = frustum_corners[0];
    auto frustum_max_point = frustum_corners[0];
    for (auto i : vstd::range(1, 8)) {
        frustum_min_point = min(frustum_min_point, frustum_corners[i]);
        frustum_max_point = max(frustum_max_point, frustum_corners[i]);
    }
    static bool bb = false;
    auto frustum_cull_callback = [&](float4x4 const &transform, AABB const &bounding) {
        return frustum_cull(transform, bounding, frustum_planes, frustum_min_point, frustum_max_point, ctx.cam.dir_forward(), make_float3(ctx.cam.position));
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
    emission = render_device.create_transient_image<float>("emission", PixelStorage::BYTE4, frame_settings.render_resolution, 1, false, true);
    frame_settings.resolved_img = &emission;
    auto id_map = render_device.create_transient_image<uint>("id_map", PixelStorage::INT4, frame_settings.render_resolution, 1, false, true);
    cmdlist << pass_ctx->depth_buffer.clear(0.0f)
            << (*_clear_id)(id_map, uint4(-1, -1, 0, 0)).dispatch(frame_settings.render_resolution);
    //
    if (!draw_meshes.empty()) {
        raster::VertArgs vert_args{
            .view = cam_data.view,
            .proj = cam_data.proj,
            .view_proj = cam_data.vp};
        // cmdlist << _draw_scene_shader(data_buffer, vert_args)
        //                .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, emission);
        cmdlist << (*_draw_id_shader)(data_buffer, vert_args)
                       .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, id_map);
    }
    cmdlist << (*_shading_id)(id_map, emission).dispatch(frame_settings.render_resolution);
    auto click_manager = ctx.pipeline_settings->read_if<ClickManager>();
    if (click_manager) {
        if (!click_manager->_contour_objects.empty()) {
            contour(ctx, click_manager->_contour_objects);
            click_manager->_contour_objects.clear();
        }
        if (!click_manager->_requires.empty()) {
            // click
            auto &reqs = click_manager->_requires;
            auto require_buffer = sm.host_upload_buffer().allocate_upload_buffer<float2>(reqs.size());
            auto result_buffer = render_device.create_transient_buffer<RayCastResult>(
                "click_result",
                reqs.size());
            luisa::fixed_vector<float2, 2> req_coords;
            luisa::fixed_vector<RayCastResult, 2> result;
            result.push_back_uninitialized(reqs.size());
            vstd::push_back_func(req_coords, reqs.size(), [&](size_t i) {
                return reqs[i].second.screen_uv;
            });
            std::memcpy(require_buffer.mapped_ptr(), req_coords.data(), req_coords.size_bytes());
            cmdlist << click_pick::dispatch_shader(_click_pick, reqs.size(), sm.buffer_heap(), require_buffer.view, id_map, result_buffer.view())
                    << result_buffer.view().copy_to(result.data());
            cmdlist.add_callback([&ctx,
                                  reqs = std::move(reqs),
                                  result = std::move(result)]() mutable {
                auto click_manager = ctx.pipeline_settings->read_if<ClickManager>();
                if (!click_manager) return;
                std::lock_guard lck{click_manager->_mtx};
                for (auto i : vstd::range(reqs.size())) {
                    click_manager->_results.force_emplace(std::move(reqs[i].first), result[i]);
                }
            });
        }
        // frame selection
        luisa::vector<uint> selection_result;
        luisa::spin_mutex selection_mtx;
        click_manager->_mtx.lock();
        auto frame_selection = std::move(click_manager->_frame_selection_requires);
        click_manager->_mtx.unlock();
        for (auto &i : frame_selection) {
            if (length(abs(i.second.xy() - i.second.zw())) < 1e-3f) {
                continue;
            }
            selection_result.clear();
            auto select_call = [&](uint user_id, float4x4 const &transform, AABB const &bounding_box) {
                if (!frustum_cull(transform, bounding_box, frustum_planes, frustum_min_point, frustum_max_point, ctx.cam.dir_forward(), make_float3(ctx.cam.position))) {
                    return false;
                }
                float3 local_min(bounding_box.packed_min[0], bounding_box.packed_min[1], bounding_box.packed_min[2]);
                float3 local_max(bounding_box.packed_max[0], bounding_box.packed_max[1], bounding_box.packed_max[2]);
                float3 proj_min = make_float3(1e20f, 1e20f, 1.f);
                float3 proj_max = make_float3(-1e20f, -1e20f, 0.f);
                for (int x = 0; x < 2; x++)
                    for (int y = 0; y < 2; y++)
                        for (int z = 0; z < 2; z++) {
                            float3 point = select(local_min, local_max, int3(x, y, z) == 1);
                            float4 proj = cam_data.vp * (transform * make_float4(point, 1.f));
                            proj /= proj.w;
                            proj_min = min(proj_min, proj.xyz());
                            proj_max = max(proj_max, proj.xyz());
                        }
                if (all(proj_max < make_float3(i.second.zw(), 1e20f) && proj_min > make_float3(i.second.xy(), 0.f))) {
                    std::lock_guard lck{selection_mtx};
                    selection_result.push_back(user_id);
                    return true;
                }
                return false;
            };
            sm.accel_manager().iterate_scene(select_call);
            if (!selection_result.empty()) {
                std::lock_guard lck{click_manager->_mtx};
                click_manager->_frame_selection_results.force_emplace(std::move(i.first), std::move(selection_result));
            }
        }
    }
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
