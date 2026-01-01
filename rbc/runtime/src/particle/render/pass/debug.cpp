/**
 * @file debug.cpp
 * @brief The Impl for Debug Pass
 * @author sailing-innocent
 * @date 2024-05-17
 */

#include "rbc_particle/render/pass/debug.h"
#include "rbc_particle/render/pipeline.h"
#include <iostream>

namespace rbc::ps {

DebugPSPass::~DebugPSPass() = default;

void DebugPSPass::on_enable(PSPipeline const &pipeline, Device &device, CommandList &cmdlist, Scene &scene) {
    Kernel2D<Image<float>, int, int> debug_kernel = [&](ImageVar<float> dst_img, Int res_w, Int res_h) {
        auto coord = dispatch_id().xy();
        $if (coord.x >= res_w | coord.y >= res_h) { return; };
        auto u = Float(coord.x) / Float(res_w);
        auto v = Float(coord.y) / Float(res_h);

        dst_img.write(coord, make_float4(u, v, 0.f, 1.f));
    };
    m_debug_shader = device.compile(debug_kernel, ShaderOption{.name = "debug"});
}
void DebugPSPass::update(PSPipeline const &pipeline, PSPipelineContext &ctx) {
    // fetch pass context ctx.mut.m_pass_contexts.find(name)
    // find the binded render state proxy names in PSPassContext
    // for name in pass_ctx.emit_render_states
    /// find the render state proxy in scene.emit_render_states[name]
    /// dispatch with passes own rule

    // dispatch
    auto res = ctx.resolution;
    ctx.cmdlist << (m_debug_shader)((*ctx.dst_img), res.x, res.y).dispatch(res);
}

void DebugPSPass::on_disable(PSPipeline const &pipeline) {
}

}// namespace rbc::ps