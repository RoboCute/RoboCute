#pragma once

/**
 * @file debug.h
 * @brief The Debug Pass
 * @author sailing-innocent
 * @date 2024-05-17
 */

#include "rbc_particle/render/pass.h"
#include <luisa/luisa-compute.h>

namespace rbc::ps {

class DebugPSPass : public PSPass {
    template<typename T>
    using Image = luisa::compute::Image<T>;

    template<size_t Dim, typename... Args>
    using Shader = luisa::compute::Shader<Dim, Args...>;

    Shader<2, Image<float>, int, int> m_debug_shader;

public:
    void on_enable(PSPipeline const &pipeline, Device &device, CommandList &cmdlist, Scene &scene) override;
    void update(PSPipeline const &pipeline, PSPipelineContext &ctx) override;
    void on_disable(PSPipeline const &pipeline) override;
    ~DebugPSPass();
};

}// namespace rbc::ps