#include <rbc_render/post_pass.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/scene_manager.h>
#include <luisa/runtime/context.h>
#include <rbc_graphics/render_device.h>
#include <rbc_render/renderer_data.h>

namespace rbc {
struct PostPassContext : public PassContext {
    // FrameGen frame_gen;
    ACES aces;
    Exposure exposure;
    bool reset : 1 {true};
    bool aces_lut_dirty : 1 {true};
    PostPassContext(
        Device &device,
        luisa::fiber::counter &init_counter,
        filesystem::path const &path,
        Pipeline const &pipeline,
        BufferUploader &uploader,
        float globalExposure,
        uint2 res,
        float minLuminance, float maxLuminance, bool use_hdr)
        // : frame_gen(path, device, res, minLuminance, maxLuminance, use_hdr)
        : aces(init_counter, true), exposure(device, init_counter, res) {
        float data[]{
            globalExposure,
            0.f,
            1.0f,
            0.0f};
        uploader.emplace_copy_cmd(
            BufferView<float>{exposure.exposure_buffer},
            data);
    }
    ~PostPassContext() = default;
};
}// namespace rbc
RBC_RTTI(rbc::PostPassContext)
namespace rbc {
namespace detail {
void post_process_distortion(float4 &distortion_CenterScale, float4 &distortion_Amount, DistortionSettings const &dis) {
    float amount = 1.6f * max(abs(dis.intensity), 1.f);
    float theta = min(160.f, amount) * pi / 180.0f;
    float sigma = 2.f * tan(theta * 0.5f);
    distortion_CenterScale = float4(dis.center.x, dis.center.y, max(dis.intensity_multiplier.x, 1e-4f), max(dis.intensity_multiplier.y, 1e-4f));
    distortion_Amount = float4(dis.intensity >= 0.f ? theta : 1.f / theta, sigma, 1.f / dis.scale, dis.intensity);
}
}// namespace detail
PostPass::PostPass(DeviceConfigExt *device_config)
    : device_config(device_config) {
}

PostPass::~PostPass() {
}

void PostPass::on_enable(Pipeline const &pipeline, Device &device, CommandList &cmdlist, SceneManager &scene) {
    // Callable linear_to_srgb = [&](Var<float3> x) noexcept {
    // 	return saturate(select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f, 12.92f * x, x <= 0.00031308f));
    // };

    // auto hdr_to_ldr = [&](ImageVar<float> hdr_image, ImageVar<float> ldr_image) {
    // 	auto coord = dispatch_id().xy();
    // 	Float3 v = hdr_image->read(coord).xyz();
    // 	v = linear_to_srgb(v);
    // 	ldr_image.write(coord, make_float4(v, 1.f));
    // };

    // hdr_to_ldr_shader = scene.shader_manager().compile<2>(device, hdr_to_ldr, ShaderOption{.name = luisa::to_string("hdr_to_ldr")});

    // GBuffer from here
    if (device_config) {
        // lpm.create(device_config, pipeline, device, cmdlist, scene, resolution);
        // lpm->load(&pipeline.db);
    }
    // CombineLUTShader
    // scene.shader_manager().load("post_process/combineLUT_Pass.bin", combineLUTShader);

    // uber_shader
    ShaderManager::instance()->async_load(init_counter, "post_process/uber.bin", uber_shader);
    ShaderManager::instance()->async_load(init_counter, "gui/blit_shader.bin", blit_shader);
    ShaderManager::instance()->async_load(init_counter, "gui/blit_from_buffer.bin", blit_from_buffer);
}

void PostPass::wait_enable() {
    init_counter.wait();
}

void PostPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    const auto &toneMappingSettings = ctx.pipeline_settings.read<ToneMappingSettings>();
    const auto &displaySettings = ctx.pipeline_settings.read<DisplaySettings>();
    const auto &frame_settings = ctx.pipeline_settings.read<FrameSettings>();
    const auto &exposureSettings = ctx.pipeline_settings.read<ExposureSettings>();
    init_counter.wait();
    auto &pipeline_mode = ctx.pipeline_settings.read<PTPipelineSettings>();
    PostPassContext *post_ctx{};
    if (!pipeline_mode.use_post_filter) {
        return;
    }
    post_ctx = ctx.mut.get_pass_context<PostPassContext>(
        (*ctx.device),
        init_counter,
        ctx.scene->ctx().runtime_directory(),
        pipeline, ctx.scene->buffer_uploader(),
        exposureSettings.globalExposure,
        frame_settings.display_resolution,
        toneMappingSettings.lpm.displayMinLuminance,
        toneMappingSettings.lpm.displayMaxLuminance,
        displaySettings.use_hdr_display);
    init_counter.wait();
    // post_ctx->reset |= frame_settings.frame_index == 0;
    auto &scene = ctx.scene;
    if (post_ctx->reset) {
        post_ctx->aces_lut_dirty = true;
    }
    post_ctx->aces_lut_dirty |= toneMappingSettings.aces.dirty;
    if (post_ctx->aces_lut_dirty) {
        post_ctx->aces.early_render(toneMappingSettings.aces, (*ctx.device), (*ctx.cmdlist), ctx.scene->host_upload_buffer());
    }
}

void PostPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    const auto &distortionSettings = ctx.pipeline_settings.read<DistortionSettings>();
    auto &toneMappingSettings = ctx.pipeline_settings.read_mut<ToneMappingSettings>();
    const auto &displaySettings = ctx.pipeline_settings.read<DisplaySettings>();
    const auto &exposureSettings = ctx.pipeline_settings.read<ExposureSettings>();
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    auto &render_device = RenderDevice::instance();

    ///////////// recycle unused gbuffer
    ///////////// recycle unused gbuffer
    Image<float> temp_img;
    auto &cmdlist = (*ctx.cmdlist);
    if (!frame_settings.resolved_img) {
        if (frame_settings.radiance_buffer) {
            temp_img = render_device.create_transient_image<float>("temp_tex_from_radiance", PixelStorage::FLOAT4, frame_settings.display_resolution);
            cmdlist << (*blit_from_buffer)(temp_img, *frame_settings.radiance_buffer, 3).dispatch(frame_settings.display_resolution);
            frame_settings.resolved_img = &temp_img;
        } else {
            return;
        }
    }
    auto &pipeline_mode = ctx.pipeline_settings.read<PTPipelineSettings>();
    if (!pipeline_mode.use_post_filter) {
        cmdlist << (*blit_shader)(*frame_settings.dst_img, *frame_settings.resolved_img, false).dispatch(frame_settings.dst_img->size());
        return;
    }
    auto &scene = *ctx.scene;

    toneMappingSettings.aces.dirty = false;

    auto temp_res = render_device.create_transient_image<float>(
        "post_temp_img",
        frame_settings.resolved_img->storage(),
        frame_settings.resolved_img->size());
    Image<float> const *imgs[2]{
        frame_settings.resolved_img,
        &temp_res,
    };

    auto read_tex = [&]() -> auto & {
        return *imgs[0];
    };
    auto write_tex = [&]() -> auto & {
        return *imgs[1];
    };
    auto swap_tex = [&]() {
        std::swap(imgs[0], imgs[1]);
    };
    post_uber_pass::Args args{};
    ///////// distortion
    rbc::detail::post_process_distortion(args.distortion_CenterScale, args.distortion_Amount, distortionSettings);
    args.chromatic_aberration = displaySettings.chromatic_aberration;
    args.pixel_offset = frame_settings.display_offset;
    auto &post_ctx = ctx.mut.get_pass_context_mut<PostPassContext>();
    post_ctx->exposure.generate(
        exposureSettings,
        cmdlist,
        read_tex(),
        frame_settings.display_resolution,
        post_ctx->reset, frame_settings.delta_time);

    if (post_ctx->aces_lut_dirty) {
        post_ctx->aces.dispatch(toneMappingSettings.aces, cmdlist);
        post_ctx->aces_lut_dirty = false;
    }
    args.saturate_result = !displaySettings.use_hdr_display;
    args.gamma = displaySettings.use_hdr_display || displaySettings.use_linear_sdr ? 1.0f : 1.0f / displaySettings.gamma;// TODO
    args.use_hdr10 = displaySettings.use_hdr_10;
    args.hdr_display_multiplier = displaySettings.use_hdr_display ? toneMappingSettings.aces.tone_mapping.hdr_display_multiplier : 1.0f;
    float hdr_input_multiplier = 1.0f;
    if (displaySettings.use_hdr_display) {
        hdr_input_multiplier = toneMappingSettings.aces.tone_mapping.hdr_paper_white / toneMappingSettings.lpm.displayMaxLuminance;
    }
    args.hdr_input_multiplier = hdr_input_multiplier;
    // args.localExposure_detail_strength = pipeline.settings.exposure.localExposureDetail;
    Image<float> const *uber_out_img = frame_settings.dst_img;
    LUISA_ASSERT(read_tex(), "Bad read");
    LUISA_ASSERT(post_ctx->aces.lut3d_volume, "Bad lut3d");
    auto lpm_args = LPM::compute(toneMappingSettings.lpm);
    cmdlist << (*uber_shader)(
                   read_tex(),
                   post_ctx->aces.lut3d_volume,
                   //    post_ctx->exposure.local_exp_volume,
                   args,
                   lpm_args,
                   post_ctx->exposure.exposure_buffer,
                   *uber_out_img)
                   .dispatch(frame_settings.display_resolution);
    if (post_ctx)
        post_ctx->reset = false;
}

void PostPass::on_frame_end(Pipeline const &pipeline, Device &device, SceneManager &scene) {
}

void PostPass::on_disable(Pipeline const &pipeline, Device &device, CommandList &cmdlist, SceneManager &scene) {
}
}// namespace rbc