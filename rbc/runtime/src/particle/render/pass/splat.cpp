/**
 * @file splat.cpp
 * @brief The Splat Pass
 * @author sailing-innocent
 * @date 2024-05-20
 */

#include "rbc_particle/render/pass/splat.h"
#include "rbc_particle/render/pipeline.h"

namespace rbc::ps {

SplatPSPass::~SplatPSPass() = default;
void SplatPSPass::on_enable(PSPipeline const &pipeline, Device &device, CommandList &cmdlist, Scene &scene) {
    Kernel1D<Image<float>, Buffer<float>, int, int> kernel = [&](ImageVar<float> dst_img, BufferVar<float> pos, Int res_w, Int res_h) {
        auto idx = dispatch_id().x;
        auto px = pos.read(3 * idx + 0);
        auto py = pos.read(3 * idx + 1);
        auto pz = pos.read(3 * idx + 2);
        Float wf = (Float)res_w;
        Float hf = (Float)res_h;

        Float4 base_color = {1.0f, 1.0f, 1.0f, 1.0f};

        for (auto i = -2; i <= 2; i++) {
            for (auto j = -2; j <= 2; j++) {
                auto pos = make_int2(static_cast<Int>(px * wf), static_cast<Int>(py * hf)) +
                           make_int2(i, j);

                auto hix = static_cast<Int>(wf * 0.5f);
                auto hiy = static_cast<Int>(hf * 0.5f);
                $if (pos.x >= -hix & pos.x < hix & pos.y >= -hiy & pos.y < hiy) {
                    dst_img.write(
                        make_uint2(static_cast<UInt>(pos.x + hix), static_cast<UInt>(hiy - pos.y - 1)),
                        base_color);
                };
            }
        }
    };
    m_splat_shader = device.compile(kernel, ShaderOption{.name = "splat"});
}

void SplatPSPass::early_update(PSPipeline const &pipeline, PSPipelineContext &ctx) {
    mp_ctx = ctx.mut.get_pass_context<SplatPSPassContext>("splat");
}

void SplatPSPass::update(PSPipeline const &pipeline, PSPipelineContext &ctx) {
    auto res = ctx.resolution;
    // [TODO]: fetch emitter scene node id from mp_ctx->
    for (auto scene_node_id : mp_ctx->emitter_scene_node_indices) {
        auto &scene_node = ctx.scene.get_emit_node(scene_node_id);
        // LUISA_INFO("get scene node {}", scene_node_id);
        auto &proxy = scene_node.m_render_state_proxy;
        auto buf_iter = proxy.buffers.find("pos");
        if (buf_iter) {
            auto pos_buf_view = buf_iter.value().uint_view.as<float>();
            ctx.cmdlist << (m_splat_shader)(*ctx.dst_img, pos_buf_view, res.x, res.y).dispatch(proxy.view_size);
        } else {
            LUISA_ERROR("GET POS FAILED");
        }
    }
}

void SplatPSPass::on_disable(PSPipeline const &pipeline) {
}

}// namespace rbc::ps