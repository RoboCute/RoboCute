#include <rbc_render/pipeline.h>
#include <rbc_render/accum_pass.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/offline_pt_pass.h>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>

// #include <oidn_denoiser.h>
namespace rbc {
// namespace lut_baker
// {
// #include <path_tracer/lut_gen.inl>
// }
void AccumPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    // scene.shader_manager().load("path_tracer/nrd_modulate.bin", modulate);
    auto load = [&](auto name, auto &&var) {
        ShaderManager::instance()->async_load(init_counter, name, var);
    };
    load("path_tracer/accum/offline_accum.bin", accum);
    load("path_tracer/accum/offline_accum_tex2buffer.bin", tex_to_buffer);
    load("path_tracer/accum/offline_accum_buffer2tex_blend.bin", buffer_to_tex_blend);
    // _lut_baker = lut_baker::load_shader("path_tracer/lut_gen.bin");
}
void AccumPass::wait_enable() {
    init_counter.wait();
}
void AccumPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto &jitter_data = ctx.pipeline_settings->read_mut<JitterData>();

    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    pass_ctx = ctx.mut.get_pass_context<AccumPassContext>();
    pass_ctx->frame_index = std::min<size_t>(pass_ctx->frame_index, frameSettings.frame_index);
    if (any(frameSettings.render_resolution != frameSettings.display_resolution)) {
        pass_ctx->frame_index = 0;
    }
    auto &hdr = pass_ctx->hdr;
    if (hdr && any(hdr.size() != frameSettings.display_resolution)) {
        hdr.reset();
    }
    if (!hdr) {
        hdr = ctx.device->create_image<float>(PixelStorage::FLOAT4, frameSettings.display_resolution);
    }
    auto &mut = ctx.mut;
    auto halton = [](uint i, uint b) {
        float f = 1.0f;
        float invB = 1.0f / b;
        float r = 0.0f;
        while (i > 0u) {
            f = f * invB;
            r = r + f * (i % b);
            i = i / b;
        };
        return r;
    };
    jitter_data.jitter_phase_count = ~0u;
    jitter_data.jitter = float2(halton(pass_ctx->frame_index & (jitter_data.jitter_phase_count - 1), 2), halton(pass_ctx->frame_index & (jitter_data.jitter_phase_count - 1), 3)) - 0.5f;
}
Image<float> const *AccumPass::copy_hdr_img_to_buffer(
    Pipeline const &pipeline,
    PipelineContext const &ctx,
    CommandList &cmdlist,
    Buffer<float> &buffer) {
    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    auto pass_ctx = ctx.mut.get_pass_context<AccumPassContext>();
    cmdlist << (*tex_to_buffer)(
                   pass_ctx->hdr,
                   buffer)
                   .dispatch(frameSettings.display_resolution);
    return &pass_ctx->hdr;
}
Image<float> const *AccumPass::copy_buffer_to_hdr_img(
    Pipeline const &pipeline,
    PipelineContext const &ctx,
    CommandList &cmdlist,
    Buffer<float> &noisy_buffer,
    Buffer<float> &denoise_buffer,
    float denoise_weight) {
    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    auto pass_ctx = ctx.mut.get_pass_context<AccumPassContext>();
    cmdlist << (*buffer_to_tex_blend)(noisy_buffer, denoise_buffer, pass_ctx->hdr, denoise_weight).dispatch(frameSettings.display_resolution);
    return &pass_ctx->hdr;
}
void AccumPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto &pt_pass_ctx = ctx.mut.get_pass_context_mut<PTPassContext>();
    if (pt_pass_ctx && pt_pass_ctx->noisy_initialized && pass_ctx->hdr) {
        ctx.mut.resolved_img = &pass_ctx->hdr;
    } else {
        auto &render_device = RenderDevice::instance();
        const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
        auto &scene = *ctx.scene;
        auto emission = render_device.create_transient_image<float>("emission", PixelStorage::FLOAT4, frameSettings.render_resolution);
        temp_img = render_device.create_transient_image<float>("accum_temp_img", PixelStorage::FLOAT4, frameSettings.display_resolution);
        (*ctx.cmdlist) << (*accum)(emission, pass_ctx->hdr, temp_img, frameSettings.render_resolution, pass_ctx->frame_index).dispatch(frameSettings.display_resolution);
        ctx.mut.resolved_img = &temp_img;
    }

    /////// Bake lut
    // constexpr uint64_t lut_frame = 16384;
    // const uint3 lut_dispatch_size(32);
    // if (!lut_buffer)
    // {
    //     buffer_frame_idx = 0;
    //     lut_buffer = ctx.device->create_buffer<float4>(lut_dispatch_size.x * lut_dispatch_size.y * lut_dispatch_size.z);
    // }
    // for (int i = 0; i < 16; ++i)
    // {
    //     if (buffer_frame_idx < lut_frame)
    //     {
    //         (*ctx.cmdlist) << lut_baker::dispatch_shader(_lut_baker, lut_dispatch_size, ctx.scene->image_heap(), lut_buffer, prepare_pass->illum_d65_idx, prepare_pass->cie_xyz_cdfinv_idx, prepare_pass->spectrum_lut3d_idx, buffer_frame_idx);
    //     }
    //     else if (buffer_frame_idx == lut_frame)
    //     {
    //         luisa::vector<std::byte> lut_result;
    //         lut_result.push_back_uninitialized(lut_buffer.size_bytes());
    //         (*ctx.cmdlist) << lut_buffer.view().copy_to(lut_result.data());
    //         ctx.cmdlist->add_callback([lut_result = std::move(lut_result), lut_dispatch_size]() {
    //             auto path = "my_lut3d.bytes"sv;
    //             BinaryFileWriter bfs(luisa::string{ path });
    //             bfs.write(lut_result);

    //             // for (size_t i = 0; i < lut_dispatch_size.z; ++i)
    //             // {
    //             //     std::string path = "../my_lut3d" + std::to_string(i) + ".hdr";
    //             //     stbi_write_hdr(path.c_str(), lut_dispatch_size.x, lut_dispatch_size.y, 4, reinterpret_cast<float const*>(lut_result.data()) + i * lut_dispatch_size.x * lut_dispatch_size.y * 4);
    //             // }

    //             LUISA_INFO("Write lut to {}", path);
    //         });
    //     }
    //     ++buffer_frame_idx;
    // }

    pass_ctx->frame_index++;
}
void AccumPass::on_frame_end(
    Pipeline const &pipeline,
    Device &device,
    SceneManager &scene) {
}
void AccumPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
}
}// namespace rbc