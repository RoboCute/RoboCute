#include <rbc_render/post_pass.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/scene_manager.h>
#include <luisa/runtime/context.h>
#include <rbc_graphics/render_device.h>

namespace rbc {
struct PostPassContext : public PassContext {
    // FrameGen frame_gen;
    ACES aces;
    Exposure exposure;
    bool reset{true};
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
    aces_lut_dirty = true;
}

void PostPass::wait_enable() {
    init_counter.wait();
}

BufferView<float> PostPass::exposure_buffer() const {
    return post_ctx->exposure.exposure_buffer;
}

void PostPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    const auto &toneMappingSettings = ctx.pipeline_settings->read<ToneMappingSettings>();
    const auto &displaySettings = ctx.pipeline_settings->read<DisplaySettings>();
    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    const auto &exposureSettings = ctx.pipeline_settings->read<ExposureSettings>();

    post_ctx = ctx.mut.get_pass_context<PostPassContext>(
        (*ctx.device),
        init_counter,
        ctx.scene->ctx().runtime_directory(),
        pipeline, ctx.scene->buffer_uploader(),
        exposureSettings.globalExposure,
        frameSettings.display_resolution,
        toneMappingSettings.lpm.displayMinLuminance,
        toneMappingSettings.lpm.displayMaxLuminance,
        displaySettings.use_hdr_display);
    init_counter.wait();
    post_ctx->reset |= frameSettings.frame_index == 0;
    auto &scene = ctx.scene;
    if (post_ctx->reset) {
        aces_lut_dirty = true;
    }
    aces_lut_dirty |= toneMappingSettings.aces.dirty;
    if (aces_lut_dirty) {
        post_ctx->aces.early_render(toneMappingSettings.aces, (*ctx.device), (*ctx.cmdlist), ctx.scene->host_upload_buffer());
    }
}

void PostPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    const auto &distortionSettings = ctx.pipeline_settings->read<DistortionSettings>();
    auto &toneMappingSettings = ctx.pipeline_settings->read_mut<ToneMappingSettings>();
    const auto &displaySettings = ctx.pipeline_settings->read<DisplaySettings>();
    const auto &exposureSettings = ctx.pipeline_settings->read<ExposureSettings>();
    auto &frameSettings = ctx.pipeline_settings->read_mut<FrameSettings>();
    auto &render_device = RenderDevice::instance();

    ///////////// recycle unused gbuffer
    ///////////// recycle unused gbuffer
    if (!frameSettings.resolved_img) [[unlikely]] {
        LUISA_ERROR("Resolved image is empty");
    }
    auto &scene = *ctx.scene;

    toneMappingSettings.aces.dirty = false;

    auto temp_res = render_device.create_transient_image<float>(
        "post_temp_img",
        frameSettings.resolved_img->storage(),
        frameSettings.resolved_img->size());
    Image<float> const *imgs[2]{
        frameSettings.resolved_img,
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
    args.pixel_offset = frameSettings.display_offset;

    post_ctx->exposure.generate(
        exposureSettings,
        (*ctx.cmdlist),
        read_tex(),
        frameSettings.display_resolution,
        post_ctx->reset, frameSettings.delta_time);

    if (aces_lut_dirty) {
        post_ctx->aces.dispatch(toneMappingSettings.aces, (*ctx.cmdlist));
        aces_lut_dirty = false;
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
    Image<float> const *uber_out_img = frameSettings.dst_img;
    LUISA_ASSERT(read_tex(), "Bad read");
    LUISA_ASSERT(post_ctx->aces.lut3d_volume, "Bad lut3d");
    auto lpm_args = LPM::compute(toneMappingSettings.lpm);
    (*ctx.cmdlist) << (*uber_shader)(
                          read_tex(),
                          post_ctx->aces.lut3d_volume,
                          //    post_ctx->exposure.local_exp_volume,
                          args,
                          lpm_args,
                          post_ctx->exposure.exposure_buffer,
                          *uber_out_img)
                          .dispatch(frameSettings.display_resolution);
}

void PostPass::on_frame_end(Pipeline const &pipeline, Device &device, SceneManager &scene) {
    post_ctx->reset = false;
}

void PostPass::on_disable(Pipeline const &pipeline, Device &device, CommandList &cmdlist, SceneManager &scene) {
}
}// namespace rbc