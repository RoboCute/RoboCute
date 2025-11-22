#include <rbc_render/offline_pt_pass.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/utils/heitz_sobol.h>
#include <rbc_render/accum_pass.h>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc {
namespace offline_pt_shader {
#include <path_tracer/offline_pt.inl>
}// namespace offline_pt_shader
namespace offline_pt_shader_denoise {
#include <path_tracer/offline_pt_denoise.inl>
}// namespace offline_pt_shader_denoise
namespace offline_multibounce {
#include <path_tracer/pt_multi_bounce_offline.inl>
}// namespace offline_multibounce
PTPassContext::PTPassContext() {
#ifdef OUTPUT_DENOISE_TRAINING_DATA
    denoise_buffer = device.create_buffer<DenoiseTrainingData>(res.x * res.y);
#endif
}
PTPassContext::~PTPassContext() = default;
void OfflinePTPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
#define RBQ_LOAD_SHADER(SHADER_NAME, NAME_SPACE, PATH) \
    init_counter.add();                                \
    luisa::fiber::schedule([this]() {                  \
        SHADER_NAME = NAME_SPACE::load_shader(PATH);   \
        init_counter.done();                           \
    })

    auto load = [&](auto name, auto &&var) {
        init_counter.add();
        luisa::fiber::schedule([this, &scene, name, &var]() {
            ShaderManager::instance()->load(name, var);
            init_counter.done();
        });
    };
    // luisa::vector<luisa::string> variants{ "ltc_sheen_brdf",
    //                                        "dielectric_coat_brdf",
    //                                        "sub_surface_bsdf",
    //                                        "diffraction_grating_brdf" };
    // pt_shader.init(
    //     offline_pt_shader::load_shader,
    //     "path_tracer",
    //     "path_tracer/pt_variants",
    //     variants,
    //     "offline_pt"
    // );
    // pt_shader_denoise.init(
    //     offline_pt_shader_denoise::load_shader,
    //     "path_tracer",
    //     "path_tracer/pt_variants",
    //     variants,
    //     "offline_pt_denoise"
    // );
    // multi_bounce.init(
    //     offline_multibounce::load_shader,
    //     "path_tracer",
    //     "path_tracer/pt_variants",
    //     variants,
    //     "pt_multi_bounce_offline"
    // );
    // pt_shader.load_all(init_counter);
    // pt_shader_denoise.load_all(init_counter);
    // multi_bounce.load_all(init_counter);
    RBQ_LOAD_SHADER(pt_shader, offline_pt_shader, "path_tracer/offline_pt.bin");
    RBQ_LOAD_SHADER(pt_shader_denoise, offline_pt_shader_denoise, "path_tracer/offline_pt_denoise.bin");
    RBQ_LOAD_SHADER(multi_bounce, offline_multibounce, "path_tracer/pt_multi_bounce_offline.bin");
    load("path_tracer/draw_sky.bin", draw_sky_shader);
    load("surfel/clear_hashgrid_offline.bin", clear_hashgrid);
    load("surfel/accum_hashgrid_offline.bin", accum_hashgrid);
    load("surfel/integrate_hashgrid_offline.bin", integrate_hashgrid);
    load("path_tracer/clear_buffer.bin", clear_ptr_buffer);
    init_counter.add();
    luisa::fiber::schedule([&]() {
        auto hash_size = 1024ull * 1024ull * 4ull;
        key_buffer = device.create_buffer<uint>(hash_size);
        value_buffer = device.create_buffer<uint>(hash_size * 4);
        init_counter.done();
    });
#undef RBQ_LOAD_SHADER
    // click_buffer = device.create_buffer<uint2>(1);
}
void OfflinePTPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    if (!ctx.scene->accel()) {
        ctx.scene->accel_manager().init_accel(*ctx.cmdlist);
    }
}
void OfflinePTPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto accum_pass_ctx = ctx.mut.get_pass_context<AccumPassContext>();
    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    auto &scene = *ctx.scene;
    auto &cmdlist = (*ctx.cmdlist);
    auto &accel = scene.accel();
    auto &render_device = RenderDevice::instance();
    auto emission = render_device.create_transient_image<float>("emission", PixelStorage::FLOAT4, frameSettings.render_resolution);

    const auto &jitter_data = ctx.pipeline_settings->read<JitterData>();
    const auto &cam_data = ctx.pipeline_settings->read<CameraData>();
    const auto &ptSettings = ctx.pipeline_settings->read<PathTracerSettings>();

    if (!accel || accel.size() == 0) {
        cmdlist << (*draw_sky_shader)(
                       emission,
                       scene.image_heap(),
                       scene.volume_heap(),
                       sky_heap_idx,
                       frameSettings.to_rec2020_matrix,
                       cam_data.world_to_sky,
                       cam_data.inv_vp,
                       make_float3(ctx.cam.position),
                       jitter_data.jitter,
                       frameSettings.frame_index)
                       .dispatch(frameSettings.render_resolution);
        return;
    }
    auto surfel_mark = render_device.create_transient_image<uint>("surfel_mask", PixelStorage::INT1, frameSettings.render_resolution);
    auto &pass_ctx = ctx.mut.get_pass_context_mut<PTPassContext>();
    Buffer<offline::MultiBouncePixel> multibounce_buffer;
    Buffer<uint> multibounce_buffer_counter;
    {
        uint2 res = frameSettings.display_resolution;
        auto buffer_size = ((res + 1u) / 2u);
        multibounce_buffer = render_device.create_transient_buffer<offline::MultiBouncePixel>("offline_multibounce", buffer_size.x * buffer_size.y);
        multibounce_buffer_counter = render_device.create_transient_buffer<uint>("offline_multibounce_counter", 1);
    }

    Buffer<pt::GBuffer> geo_buffer = render_device.create_transient_buffer<pt::GBuffer>("offline_geo_buffer", frameSettings.display_resolution.x * frameSettings.display_resolution.y);
    if (all(frameSettings.display_resolution == frameSettings.render_resolution) && accum_pass_ctx->frame_index < 8) {
        scene.tex_streamer().force_sync();
    }

    if (!pass_ctx) {
        pass_ctx = vstd::make_unique<PTPassContext>();
    }
    auto halton = [](int32_t index, int32_t base) {
        float f = 1.0f, result = 0.0f;

        for (int32_t currentIndex = index; currentIndex > 0;) {

            f /= (float)base;
            result = result + f * (float)(currentIndex % base);
            currentIndex = (uint32_t)(floorf((float)(currentIndex) / (float)(base)));
        }

        return result;
    };
    ////////// Physical camera

    offline::PTArgs pt_args{
        .resource_to_rec2020_mat = frameSettings.to_rec2020_matrix,
        .world_2_sky_mat = cam_data.world_to_sky,
        .sky_heap_idx = sky_heap_idx,
        .sky_lum_idx = sky_lum_idx,
        .alias_table_idx = alias_heap_idx,
        .pdf_table_idx = pdf_heap_idx,
        .cam_pos = make_float3(ctx.cam.position),
        .inv_view = cam_data.inv_view,
        .view = cam_data.view,
        .inv_vp = cam_data.inv_vp,
        .frame_countdown = scene.tex_streamer().countdown(),
        .light_count = static_cast<uint>(scene.light_accel().light_count()),
        .tex_grad_scale = float2(1),
        .enable_physical_camera = ctx.cam.enable_physical_camera,
        // .srgb_to_fourier_even_idx = prepare_pass->srgb_to_fourier_even_idx,
        // .bmese_phase_idx = prepare_pass->bmese_phase_idx,
        .time = 0.0f,
    };

    if (ctx.cam.enable_physical_camera) {
        auto lens_radius = static_cast<float>(0.05 / ctx.cam.aperture);
        auto resolution = make_float2(frameSettings.render_resolution);
        pt_args.focus_distance = ctx.cam.focus_distance;
        pt_args.lens_radius = lens_radius;
    }
    if (accum_pass_ctx->frame_index == 0) {
        cmdlist << (*clear_hashgrid)(key_buffer, value_buffer, 0).dispatch(key_buffer.size());
    }
    for (auto i : vstd::range(ptSettings.offline_spp)) {
        pt_args.bounce = ptSettings.offline_origin_bounce;
        pt_args.gbuffer_temporal_weight = 1.0f - (1.0f / float(pass_ctx->gbuffer_accumed_frame + 1));
        pt_args.reset_emission = i == 0;
        pt_args.frame_index = accum_pass_ctx->frame_index * ptSettings.offline_spp + i;
        uint max_accum = (1 + pt_args.frame_index) * 1024;
        pt_args.jitter_offset = float2(halton(pt_args.frame_index & 65535, 2), halton(pt_args.frame_index & 65535, 3));
        // output gbuffer for denoise
        cmdlist << (*clear_ptr_buffer)(multibounce_buffer_counter.view(), 0).dispatch(1);
        if (pass_ctx->albedo_buffer && pass_ctx->normal_buffer) {
            cmdlist << offline_pt_shader_denoise::dispatch_shader(
                pt_shader_denoise, ((frameSettings.render_resolution + 1u) / 2u) * 2u,
                scene.tex_streamer().level_buffer(),
                scene.buffer_heap(),
                scene.image_heap(),
                scene.volume_heap(),
                ctx.scene->accel_manager().triangle_vis_buffer(),
                accel,
#ifdef OUTPUT_DENOISE_TRAINING_DATA
                pass_ctx->denoise_buffer,
                cam_data.last_vp,
#endif
                emission,
                geo_buffer.view(),
                pass_ctx->albedo_buffer,
                pass_ctx->normal_buffer,
                scene.accel_manager().last_trans_buffer(),
                multibounce_buffer.view(),
                multibounce_buffer_counter,
                pt_args,
                frameSettings.render_resolution);
            pass_ctx->gbuffer_accumed_frame++;
            scene.dispose_queue().dispose_after_queue(std::move(pass_ctx->albedo_buffer));
            scene.dispose_queue().dispose_after_queue(std::move(pass_ctx->normal_buffer));
        } else {

            cmdlist << offline_pt_shader::dispatch_shader(
                pt_shader, ((frameSettings.render_resolution + 1u) / 2u) * 2u,
                scene.tex_streamer().level_buffer(),
                scene.buffer_heap(),
                scene.image_heap(),
                scene.volume_heap(),
                ctx.scene->accel_manager().triangle_vis_buffer(),
                accel,
#ifdef OUTPUT_DENOISE_TRAINING_DATA
                pass_ctx->denoise_buffer,
                cam_data.last_vp,
#endif
                emission,
                geo_buffer.view(),
                scene.accel_manager().last_trans_buffer(),
                multibounce_buffer.view(),
                multibounce_buffer_counter,
                pt_args,
                frameSettings.render_resolution);
            pass_ctx->gbuffer_accumed_frame = 0;
        }
        pt_args.bounce = ptSettings.offline_indirect_bounce;
        cmdlist << offline_multibounce::dispatch_shader(
            multi_bounce, multibounce_buffer.view().size(),
            scene.buffer_heap(),
            scene.image_heap(),
            scene.volume_heap(),
            scene.tex_streamer().level_buffer(),
            ctx.scene->accel_manager().triangle_vis_buffer(),
            accel,
            multibounce_buffer.view(),
            multibounce_buffer_counter,
            geo_buffer.view(),
            pt_args,
            frameSettings.render_resolution);
        cmdlist << (*accum_hashgrid)(
                       geo_buffer,
                       key_buffer,
                       value_buffer,
                       surfel_mark,
                       pt_args.jitter_offset,
                       make_float3(ctx.cam.position),
                       10.0f,
                       key_buffer.size(),
                       8,
                       max_accum)
                       .dispatch(frameSettings.render_resolution);
        cmdlist << (*integrate_hashgrid)(
#ifdef OUTPUT_DENOISE_TRAINING_DATA
                       pass_ctx->denoise_buffer,
#endif
                       geo_buffer,
                       emission,
                       value_buffer,
                       surfel_mark,
                       max_accum,
                       i == (ptSettings.offline_spp - 1) ? (1.0f / float(ptSettings.offline_spp)) : 1.0f)
                       .dispatch(frameSettings.render_resolution);
        cmdlist << (*clear_hashgrid)(key_buffer, value_buffer, max_accum).dispatch(key_buffer.size());
    }
    // TODO: spp collection
#ifdef OUTPUT_DENOISE_TRAINING_DATA
    pass_ctx->train_data.resize_uninitialized(pass_ctx->denoise_buffer.size());
    cmdlist << pass_ctx->denoise_buffer.copy_to(pass_ctx->train_data.data());
#endif
}
void OfflinePTPass::on_frame_end(
    Pipeline const &pipeline,
    Device &device,
    SceneManager &scene) {
}
void OfflinePTPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    key_buffer.reset();
    value_buffer.reset();
    // click_buffer = {};
}
void OfflinePTPass::wait_enable() {
    init_counter.wait();
}

OfflinePTPass::~OfflinePTPass() {
}
}// namespace rbc